//
//  ApiHandler.cpp
//  MusicPlayer
//
//  LocalServer 的 REST API 实现.
//
//  对应 docs/web-console/api.md 契约，并在现有 MPlayer 内核（g_player / CMediaLibrary）
//  之上落地。本文件为 plan.md §3 的“阶段 0”纵切，包含：
//    - bootstrap / library snapshot / player state / player queue
//    - player 命令端点（play/pause/next/previous/seek/volume/shuffle/repeat）
//    - songs/{id}/stream（HTTP Range 206）
//
//  注意：本处理器挂在 /api/v1 前缀，必须在 / 静态文件处理器之前注册（先注册优先）。
//

#include "ApiHandler.hpp"
#include "Connection.hpp"
#include "EventStream.hpp"
#include "Utils/Utils.h"
#include "Utils/rapidjson.h"
#include "MPlayer/Player.h"
#include "MPlayer/MediaScanner.h"
#include "LyricsLib/LyricsSearch.h"
#include "LyricsLib/CurrentLyrics.h"
#include "MediaTags/MediaTags.h"
#include <cmath>

// loopModeToString / loopModeFromString 定义于 PlayerEventSender.cpp.
cstr_t loopModeToString(int loop);
LoopMode loopModeFromString(cstr_t loop);


namespace HttpServer {

namespace {

using std::string;
using std::vector;
using rapidjson::Value;
using rapidjson::Document;

bool mediaCheapHasLyrics(Media *media);
bool findSongLyricsSource(Media *media, string &sourcePath, string &sourceType);

string uriPathUnescape(const string &s) {
    return s;
}

vector<string> splitPath(const string &path) {
    vector<string> tokens;
    size_t start = 0;
    while (start < path.size()) {
        if (path[start] == '/') {
            start++;
            continue;
        }
        size_t end = path.find('/', start);
        if (end == string::npos) {
            end = path.size();
        }
        if (end > start) {
            tokens.push_back(path.substr(start, end - start));
        }
        start = end;
    }
    return tokens;
}

string isoNow() {
    time_t t = time(nullptr);
    char buf[40];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    return buf;
}

bool eqNoCase(const string &a, const string &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}

// ---------- JSON 序列化（字段命名对齐 api.md / 既有 writeMedia） ----------

void writePlayerStatusJson(RapidjsonWriter &w) {
    w.Key("state");
    switch (g_player.getPlayerState()) {
        case PS_STOPPED: w.String("idle"); break;
        case PS_PAUSED:  w.String("paused"); break;
        case PS_PLAYING: w.String("playing"); break;
        default: assert(0); w.String("idle"); break;
    }
}

void writeMediaJson(RapidjsonWriter &w, Media *media) {
    w.StartObject();
    w.Key("id"); w.Int(media->ID);
    w.Key("artist"); w.String(media->artist.c_str());
    w.Key("album"); w.String(media->album.c_str());
    w.Key("title"); w.String(media->title.c_str());
    w.Key("year"); w.Int(media->year);
    w.Key("genre"); w.String(media->genre.c_str());
    w.Key("url"); w.String(media->url.c_str());
    w.Key("duration"); w.Int(media->duration);                       // 秒
    w.Key("fileSize"); w.Int64(media->fileSize);
    w.Key("timeAdded"); w.Int64(media->timeAdded);
    w.Key("timePlayed"); w.Int64(media->timePlayed);
    w.Key("lyricsFile"); w.String(media->lyricsFile.c_str());
    w.Key("rating"); w.Double(media->rating / 100.0);               // 0.0 ~ 5.0
    w.Key("format"); w.String(media->format.c_str());
    w.Key("play_count"); w.Int(media->countPlayed);
    w.Key("has_lyrics"); w.Bool(mediaCheapHasLyrics(media));
    w.Key("bitRate"); w.Int(media->bitRate);
    w.Key("channels"); w.Int(media->channels);
    w.Key("bitsPerSample"); w.Int(media->bitsPerSample);
    w.Key("sampleRate"); w.Int(media->sampleRate);
    w.EndObject();
}

// api.md §9 的 PlayerState 结构.
void writePlayerStateJson(RapidjsonWriter &w) {
    MediaPtr cur = g_player.getCurrentMedia();

    w.StartObject();
    writePlayerStatusJson(w);
    w.Key("player_id"); w.String("player_1");
    if (cur) {
        w.Key("song_id"); w.Int(cur->ID);
        w.Key("duration"); w.Double(g_player.getMediaLength() / 1000.0);
    } else {
        w.Key("song_id"); w.Null();
        w.Key("duration"); w.Double(0);
    }
    w.Key("position"); w.Double(g_player.getPlayPos() / 1000.0);
    w.Key("volume"); w.Double(g_player.getVolume() / 100.0);
    w.Key("shuffle"); w.Bool(g_player.isShuffle());
    w.Key("repeat"); w.String(loopModeToString(g_player.getLoop()));
    w.Key("state_version"); w.Int64(EventStream::instance().stateVersion());
    w.EndObject();
}

// 写出完整 Queue 对象 { items: [...] }，调用方只需在需要时先 Key("queue").
void writePlayerQueueJson(RapidjsonWriter &w) {
    auto playlist = g_player.getNowPlaying();
    w.StartObject();
    w.Key("items");
    w.StartArray();
    int count = playlist ? playlist->getCount() : 0;
    for (int i = 0; i < count; ++i) {
        auto media = playlist->getItem(i);
        if (!media) continue;
        w.StartObject();
        w.Key("item_id"); w.Int(i);
        w.Key("song");
        writeMediaJson(w, media.get());
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();
}

void writeLibrarySnapshotJson(RapidjsonWriter &w) {
    auto lib = g_player.getMediaLibrary();

    w.Key("version"); w.Int(g_mediaScanner.snapshotVersion());
    w.Key("generated_at"); w.String(isoNow().c_str());

    w.Key("artists"); w.StartArray();
    for (auto &a : lib->getAllArtist()) w.String(a.c_str());
    w.EndArray();

    w.Key("albums"); w.StartArray();
    for (auto &a : lib->getAllAlbum()) w.String(a.c_str());
    w.EndArray();

    w.Key("genres"); w.StartArray();
    for (auto &a : lib->getAllGenre()) w.String(a.c_str());
    w.EndArray();

    w.Key("songs"); w.StartArray();
    auto all = lib->getAll();
    int count = all->getCount();
    for (int i = 0; i < count; ++i) {
        auto media = all->getItem(i);
        if (media) writeMediaJson(w, media.get());
    }
    w.EndArray();
}

// ---------- 响应帮助 ----------

void sendJson(const ConnectionPtr &connection, const string &body, Response::StatusCode status = Response::OK) {
    auto &response = connection->response();
    response.status = status;
    response.addHeader(HEADER_CONTENT_TYPE, "application/json");
    response.addHeader(HEADER_CONTENT_LENGTH, std::to_string(body.size()));
    response.addHeader("Access-Control-Allow-Origin", "*");
    response.body = body;
    connection->sendResponse();
}

void sendNoContent(const ConnectionPtr &connection) {
    auto &response = connection->response();
    response.status = Response::NO_CONTENT;
    response.addHeader("Access-Control-Allow-Origin", "*");
    response.addHeader(HEADER_CONTENT_LENGTH, "0");
    connection->sendResponse();
}

void sendJsonError(const ConnectionPtr &connection, Response::StatusCode status, cstr_t code, cstr_t message) {
    rapidjson::StringBuffer buf;
    RapidjsonWriter w(buf);
    w.StartObject();
    w.Key("error"); w.StartObject();
    w.Key("code"); w.String(code);
    w.Key("message"); w.String(message);
    w.EndObject();
    w.EndObject();
    sendJson(connection, buf.GetString(), status);
}

void sendText(const ConnectionPtr &connection, const string &body, const string &contentType) {
    auto &response = connection->response();
    response.status = Response::OK;
    response.addHeader(HEADER_CONTENT_TYPE, contentType);
    response.addHeader(HEADER_CONTENT_LENGTH, std::to_string(body.size()));
    response.body = body;
    connection->sendResponse();
}

string mimeForMedia(cstr_t url) {
    cstr_t ext = strrchr(url, '.');
    if (ext) {
        ext++;
        if (eqNoCase(ext, "mp3")) return "audio/mpeg";
        if (eqNoCase(ext, "flac")) return "audio/flac";
        if (eqNoCase(ext, "m4a") || eqNoCase(ext, "aac") || eqNoCase(ext, "mp4")) return "audio/mp4";
        if (eqNoCase(ext, "ogg")) return "audio/ogg";
        if (eqNoCase(ext, "wav")) return "audio/wav";
    }
    return "application/octet-stream";
}

// ---------- 歌词 ----------

// 廉价的“是否有歌词”判断（用于列表中每行展示，仅做 1~2 次文件存在性检查）.
// 可靠的判断以 /songs/{id}/lyrics 接口为准.
bool mediaCheapHasLyrics(Media *media) {
    if (!media->lyricsFile.empty()) {
        if (MediaTags::isEmbeddedLyricsUrl(media->lyricsFile.c_str())) {
            return true;
        }
        if (isFileExist(media->lyricsFile.c_str())) {
            return true;
        }
    }
    if (media->url.empty()) {
        return false;
    }
    string lrc = media->url;
    fileSetExt(lrc, ".lrc");
    return isFileExist(lrc.c_str());
}

cstr_t lyricsSourceTypeOf(const string &sourcePath, bool synced) {
    if (MediaTags::isEmbeddedLyricsUrl(sourcePath.c_str())) {
        return "embedded";
    }
    return synced ? "lrc" : "txt";
}

// 定位一首歌的歌词源：
//  1) 数据库显式关联的 lyricsFile（外部文件或嵌入 URL）；
//  2) 歌曲所在目录下按 歌手/歌名 匹配的最佳歌词文件（需达到 MATCH_VALUE_OK）；
//  3) 音频内嵌歌词.
bool findSongLyricsSource(Media *media, string &sourcePath, string &sourceType) {
    sourceType.clear();
    if (!media->lyricsFile.empty()) {
        if (MediaTags::isEmbeddedLyricsUrl(media->lyricsFile.c_str())) {
            sourcePath = media->lyricsFile;
            sourceType = "embedded";
            return true;
        }
        if (isFileExist(media->lyricsFile.c_str())) {
            sourcePath = media->lyricsFile;
            return true;
        }
    }

    if (media->url.empty()) {
        return false;
    }

    CLyricsSearchParameter searchParam(media->url.c_str(), media->artist.c_str(), media->title.c_str(), false);
    ListLyrSearchResults vLyrics;

    string dir = fileGetPath(media->url.c_str());
    searchMatchLyricsInDir(dir.c_str(), searchParam, vLyrics, false);

    auto it = vLyrics.getTheBestMatchLyrics();
    if (it != vLyrics.end() && it->nMatchValue >= MATCH_VALUE_OK) {
        sourcePath = it->strUrl;
        return true;
    }

    VecStrings embedded = MediaTags::getEmbeddedLyrics(media->url.c_str());
    if (!embedded.empty()) {
        sourcePath = embedded[0];
        sourceType = "embedded";
        return true;
    }
    return false;
}

bool loadSongLyrics(Media *media, const string &sourcePath, string &contentOut, bool &syncedOut, CurrentLyrics &lyrOut) {
    int ret = lyrOut.openLyrics(media->url.c_str(), media->duration * 1000, sourcePath.c_str());
    if (ret != ERR_OK) {
        return false;
    }
    contentOut = lyrOut.toString(false);
    LyricsContentType t = lyrOut.getLyrContentType();
    syncedOut = (t == LCT_LRC || t == LCT_KARAOKE);
    return true;
}

void writeLyricsJson(const ConnectionPtr &connection, Media *media) {
    rapidjson::StringBuffer buf;
    RapidjsonWriter w(buf);
    w.StartObject();
    w.Key("song_id"); w.Int(media->ID);

    string sourcePath, hintedType;
    if (findSongLyricsSource(media, sourcePath, hintedType)) {
        string content;
        bool synced = false;
        CurrentLyrics lyr;
        if (loadSongLyrics(media, sourcePath, content, synced, lyr)) {
            w.Key("has_lyrics"); w.Bool(true);
            w.Key("synced"); w.Bool(synced);
            w.Key("source"); w.String(sourcePath.c_str());
            w.Key("source_type");
            w.String(hintedType.empty() ? lyricsSourceTypeOf(sourcePath, synced) : hintedType.c_str());
            w.Key("content"); w.String(content.c_str(), (rapidjson::SizeType)content.size());
            w.Key("lines");
            w.StartArray();
            for (auto &line : lyr.getLyricsLines()) {
                string text = line.isLyricsLine ? line.joinPiecesText() : line.content;
                if (text.empty() && !line.isLyricsLine) {
                    continue;
                }
                w.StartObject();
                w.Key("time");
                if (line.beginTime >= 0) {
                    w.Double(line.beginTime / 1000.0);
                } else {
                    w.Null();
                }
                w.Key("text"); w.String(text.c_str(), (rapidjson::SizeType)text.size());
                w.EndObject();
            }
            w.EndArray();
        } else {
            w.Key("has_lyrics"); w.Bool(false);
            w.Key("error"); w.String("PARSE_FAILED");
        }
    } else {
        w.Key("has_lyrics"); w.Bool(false);
    }
    w.EndObject();
    sendJson(connection, buf.GetString());
}

// ---------- 播放命令 ----------

void applyPlayerCommand(const char *cmd, cstr_t param) {
    if (strcmp(cmd, "play") == 0) {
        g_player.play();
    } else if (strcmp(cmd, "pause") == 0) {
        g_player.pause();
    } else if (strcmp(cmd, "next") == 0) {
        g_player.next();
    } else if (strcmp(cmd, "previous") == 0) {
        g_player.prev();
    } else if (strcmp(cmd, "seek") == 0) {
        int ms = param ? atoi(param) : -1;
        if (ms >= 0) g_player.seekTo(ms);
    } else if (strcmp(cmd, "volume") == 0) {
        // 参数支持 0~100 或 0.0~1.0.
        double v = param ? atof(param) : -1;
        if (v >= 0) {
            if (v <= 1.01) v *= 100.0;
            if (v <= 100.0) g_player.setVolume((int)v);
        }
    } else if (strcmp(cmd, "shuffle") == 0) {
        if (param) g_player.setShuffle(atoi(param) != 0);
    } else if (strcmp(cmd, "repeat") == 0) {
        if (param) g_player.setLoop(loopModeFromString(param));
    }
}

// ---------- stream ----------

void sendStream(const ConnectionPtr &connection, int id) {
    auto media = g_player.getMediaLibrary()->getMediaByID(id);
    if (!media || media->url.empty() || !isFileExist(media->url.c_str())) {
        sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "Song not found");
        return;
    }

    FilePtr fp;
    if (!fp.open(media->url, "rb")) {
        sendJsonError(connection, Response::INTERNAL_SERVER_ERROR, "INTERNAL_ERROR", "Failed to open file");
        return;
    }

    long size = fp.fileSize();
    string range;
    auto *rangeHeader = getHeaderByName(connection->request().headers, "Range");

    long start = 0, end = size - 1;
    bool partial = false;

    if (rangeHeader) {
        // 形如 "bytes=0-1234" 或 "bytes=1024-"
        string r = *rangeHeader;
        if (r.compare(0, 6, "bytes=") == 0) {
            string spec = r.substr(6);
            size_t dash = spec.find('-');
            if (dash != string::npos) {
                string a = spec.substr(0, dash);
                string b = spec.substr(dash + 1);
                if (!a.empty()) start = atol(a.c_str());
                if (!b.empty()) end = atol(b.c_str());
                if (start < 0) start = 0;
                if (end >= size) end = size - 1;
                if (end >= start) partial = true;
            }
        }
    }

    long len = end - start + 1;
    auto &response = connection->response();

    response.addHeader(HEADER_CONTENT_TYPE, mimeForMedia(media->url.c_str()));
    response.addHeader("Accept-Ranges", "bytes");

    if (partial) {
        response.status = Response::PARTIAL_CONTENT;
        response.addHeader("Content-Range", stringPrintf("bytes %ld-%ld/%ld", start, end, size));
    } else {
        response.status = Response::OK;
        start = 0;
        end = size - 1;
        len = size;
    }

    response.addHeader(HEADER_CONTENT_LENGTH, std::to_string(len));

    fp.seek(start, SEEK_SET);
    response.body = fp.read((size_t)len);

    connection->sendResponse();
}

void sendCover(const ConnectionPtr &connection, int id) {
    auto media = g_player.getMediaLibrary()->getMediaByID(id);
    if (!media || media->url.empty() || !isFileExist(media->url.c_str())) {
        sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "Song not found");
        return;
    }

    string imageData;
    if (MediaTags::getEmbeddedPicture(media->url.c_str(), 0, imageData) != ERR_OK || imageData.empty()) {
        sendJsonError(connection, Response::NOT_FOUND, "NOT_FOUND", "Cover not found");
        return;
    }

    const char *mime = "image/jpeg";
    if (imageData.size() >= 8 && (unsigned char)imageData[0] == 0x89 && imageData[1] == 'P') {
        mime = "image/png";
    } else if (imageData.size() >= 6 && imageData.compare(0, 6, "GIF87a") == 0) {
        mime = "image/gif";
    } else if (imageData.size() >= 6 && imageData.compare(0, 6, "GIF89a") == 0) {
        mime = "image/gif";
    }

    auto &response = connection->response();
    response.status = Response::OK;
    response.addHeader(HEADER_CONTENT_TYPE, mime);
    response.addHeader(HEADER_CONTENT_LENGTH, std::to_string(imageData.size()));
    response.body.swap(imageData);
    connection->sendResponse();
}

void writeScanStatusJson(RapidjsonWriter &w) {
    w.StartObject();
    w.Key("state"); w.String(g_mediaScanner.rescanStatus());
    w.Key("total"); w.Int(g_mediaScanner.rescanTotal());
    w.Key("scanned"); w.Int(g_mediaScanner.rescanDone());
    w.Key("snapshot_version"); w.Int(g_mediaScanner.snapshotVersion());
    w.EndObject();
}

int playlistDurationSeconds(Playlist *playlist) {
    int total = 0;
    int count = playlist ? (int)playlist->getCount() : 0;
    for (int i = 0; i < count; ++i) {
        auto media = playlist->getItem(i);
        if (media && media->duration > 0) {
            total += media->duration;
        }
    }
    return total;
}

void writePlaylistBriefJson(RapidjsonWriter &w, const PlaylistBrief &brief, Playlist *loaded) {
    w.StartObject();
    w.Key("id"); w.Int(brief.id);
    w.Key("name"); w.String(brief.name.c_str());
    w.Key("count"); w.Int(loaded ? (int)loaded->getCount() : brief.count);
    w.Key("duration"); w.Int(loaded ? playlistDurationSeconds(loaded) : brief.duration);
    w.Key("rating"); w.Double(brief.rating / 100.0);
    w.Key("time_modified"); w.Int64(brief.timeModified);
    w.EndObject();
}

void writePlaylistDetailJson(RapidjsonWriter &w, Playlist *playlist) {
    w.StartObject();
    w.Key("id"); w.Int(playlist->id);
    w.Key("name"); w.String(playlist->name.c_str());
    w.Key("count"); w.Int((int)playlist->getCount());
    w.Key("duration"); w.Int(playlistDurationSeconds(playlist));
    w.Key("rating"); w.Double(playlist->rating / 100.0);
    w.Key("time_modified"); w.Int64(playlist->timeModified);
    w.Key("songs");
    w.StartArray();
    int count = (int)playlist->getCount();
    for (int i = 0; i < count; ++i) {
        auto media = playlist->getItem(i);
        if (media) writeMediaJson(w, media.get());
    }
    w.EndArray();
    w.EndObject();
}

bool parseJsonBody(const ConnectionPtr &connection, Document &doc) {
    auto &body = connection->request().body;
    if (body.empty()) {
        return false;
    }
    doc.Parse(body.c_str(), body.size());
    return !doc.HasParseError() && doc.IsObject();
}

vector<int> parseSongIds(const Document &doc) {
    vector<int> ids;
    if (doc.HasMember("song_ids") && doc["song_ids"].IsArray()) {
        for (auto &v : doc["song_ids"].GetArray()) {
            if (v.IsInt()) ids.push_back(v.GetInt());
            else if (v.IsInt64()) ids.push_back((int)v.GetInt64());
        }
    } else if (doc.HasMember("song_id")) {
        if (doc["song_id"].IsInt()) ids.push_back(doc["song_id"].GetInt());
        else if (doc["song_id"].IsInt64()) ids.push_back((int)doc["song_id"].GetInt64());
    }
    return ids;
}

int queryInt(const string &uri, const string &key, int fallback) {
    size_t qpos = uri.find('?');
    if (qpos == string::npos) {
        return fallback;
    }
    string query = uri.substr(qpos + 1);
    string prefix = key + "=";
    size_t start = 0;
    while (start < query.size()) {
        size_t amp = query.find('&', start);
        if (amp == string::npos) amp = query.size();
        string part = query.substr(start, amp - start);
        if (part.compare(0, prefix.size(), prefix) == 0) {
            return atoi(part.c_str() + prefix.size());
        }
        start = amp + 1;
    }
    return fallback;
}

bool headerHasBody(Request &req) {
    for (auto &h : req.headers) {
        if (eqNoCase(h.name, "Content-Length")) {
            return atoi(h.value.c_str()) > 0;
        }
        if (eqNoCase(h.name, "Transfer-Encoding") && !eqNoCase(h.value, "identity")) {
            return true;
        }
    }
    return false;
}

} // namespace


ApiHandler::ApiHandler() : m_uriPath("/api/v1") {
}

bool ApiHandler::onRequestHeader(const ConnectionPtr &connection) {
    auto &req = connection->request();
    m_method = req.method;

    string sub = req.uri;
    size_t qpos = sub.find('?');
    if (qpos != string::npos) {
        sub = sub.substr(0, qpos);
    }
    if (sub.compare(0, m_uriPath.size(), m_uriPath) == 0) {
        sub = sub.substr(m_uriPath.size());
    }
    m_route = sub;
    auto tokens = splitPath(sub);

    if (eqNoCase(m_method, "OPTIONS")) {
        auto &response = connection->response();
        response.status = Response::OK;
        response.addHeader("Access-Control-Allow-Origin", "*");
        response.addHeader("Access-Control-Allow-Methods", "GET,POST,PUT,PATCH,DELETE,OPTIONS");
        response.addHeader("Access-Control-Allow-Headers", "Content-Type,Range");
        response.addHeader(HEADER_CONTENT_LENGTH, "0");
        connection->sendResponse();
        return true;
    }

    // 带 JSON body 的写操作留给 onRequestBody，避免 Connection 在未读完 body 时已回响应.
    if ((eqNoCase(m_method, "POST") || eqNoCase(m_method, "PUT") || eqNoCase(m_method, "PATCH"))
        && headerHasBody(req)) {
        return true;
    }

    handleRoute(connection, tokens, nullptr);
    return true;
}

bool ApiHandler::onRequestBody(const ConnectionPtr &connection) {
    auto &req = connection->request();
    auto tokens = splitPath(m_route);

    // 从 JSON body 提取该命令的参数.
    cstr_t param = nullptr;
    string paramBuf;
    if (!req.body.empty()) {
        Document doc;
        doc.Parse(req.body.c_str(), req.body.size());
        if (!doc.HasParseError() && doc.IsObject()) {
            if (tokens.size() == 2 && tokens[0] == "player" && tokens[1] == "seek") {
                if (doc.HasMember("position") && doc["position"].IsNumber()) {
                    paramBuf = std::to_string((int64_t)(doc["position"].GetDouble() * 1000.0)); // 秒 -> 毫秒
                    param = paramBuf.c_str();
                }
            } else if (doc.HasMember("parameter") && doc["parameter"].IsBool()) {
                paramBuf = doc["parameter"].GetBool() ? "1" : "0";
                param = paramBuf.c_str();
            } else if (doc.HasMember("position") && doc["position"].IsNumber()) {
                paramBuf = std::to_string((int64_t)doc["position"].GetDouble());
                param = paramBuf.c_str();
            } else if (doc.HasMember("volume") && doc["volume"].IsNumber()) {
                paramBuf = std::to_string(doc["volume"].GetDouble());
                param = paramBuf.c_str();
            } else if (doc.HasMember("shuffle") && doc["shuffle"].IsBool()) {
                paramBuf = doc["shuffle"].GetBool() ? "1" : "0";
                param = paramBuf.c_str();
            } else if (doc.HasMember("repeat") && doc["repeat"].IsString()) {
                paramBuf = doc["repeat"].GetString();
                param = paramBuf.c_str();
            }
        }
    }

    handleRoute(connection, tokens, param);
    return true;
}

void ApiHandler::handleRoute(const ConnectionPtr &connection, const vector<string> &tokens, cstr_t param) {
    if (tokens.empty()) {
        sendJsonError(connection, Response::NOT_FOUND, "NOT_FOUND", "Not found");
        return;
    }

    // GET /events (SSE)
    if (tokens.size() == 1 && tokens[0] == "events" && eqNoCase(m_method, "GET")) {
        connection->beginSse();
        EventStream::instance().addSubscriber(connection);
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlayerStateJson(w);
        EventStream::instance().sendTo(connection, "player.state_changed", buf.GetString(),
                                       EventStream::instance().stateVersion());
        {
            rapidjson::StringBuffer qbuf;
            RapidjsonWriter qw(qbuf);
            writePlayerQueueJson(qw);
            EventStream::instance().sendTo(connection, "player.queue_changed", qbuf.GetString(),
                                           EventStream::instance().stateVersion());
        }
        return;
    }

    // GET /bootstrap
    if (tokens.size() == 1 && tokens[0] == "bootstrap" && eqNoCase(m_method, "GET")) {
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        w.StartObject();
        w.Key("server"); w.StartObject(); w.Key("version"); w.String("1.0.0"); w.EndObject();
        w.Key("settings"); w.StartObject(); w.EndObject();
        w.Key("player"); writePlayerStateJson(w);
        w.Key("queue"); writePlayerQueueJson(w);
        w.Key("library"); w.StartObject();
        w.Key("snapshot_version"); w.Int(g_mediaScanner.snapshotVersion());
        auto lib = g_player.getMediaLibrary();
        w.Key("song_count"); w.Int(lib->getMediaCount());
        w.Key("album_count"); w.Int((int)lib->getAllAlbum().size());
        w.Key("artist_count"); w.Int((int)lib->getAllArtist().size());
        w.EndObject();
        w.EndObject();
        sendJson(connection, buf.GetString());
        return;
    }

    // GET /library/snapshot
    if (tokens.size() == 2 && tokens[0] == "library" && tokens[1] == "snapshot" && eqNoCase(m_method, "GET")) {
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        w.StartObject();
        writeLibrarySnapshotJson(w);
        w.EndObject();
        sendJson(connection, buf.GetString());
        return;
    }

    // GET /library/scan/status
    if (tokens.size() == 3 && tokens[0] == "library" && tokens[1] == "scan" && tokens[2] == "status"
        && eqNoCase(m_method, "GET")) {
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writeScanStatusJson(w);
        sendJson(connection, buf.GetString());
        return;
    }

    // POST /library/scan
    if (tokens.size() == 2 && tokens[0] == "library" && tokens[1] == "scan" && eqNoCase(m_method, "POST")) {
        auto all = g_player.getMediaLibrary()->getAll();
        VecMediaPtrs medias;
        int count = all ? (int)all->getCount() : 0;
        for (int i = 0; i < count; ++i) {
            auto m = all->getItem(i);
            if (m) medias.push_back(m);
        }
        if (!g_mediaScanner.startLibraryRescan(medias)) {
            sendJsonError(connection, Response::CONFLICT, "SCAN_RUNNING", "Scan already running");
            return;
        }
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writeScanStatusJson(w);
        sendJson(connection, buf.GetString(), Response::ACCEPTED);
        return;
    }

    // GET /player/state
    if (tokens.size() == 2 && tokens[0] == "player" && tokens[1] == "state" && eqNoCase(m_method, "GET")) {
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlayerStateJson(w);
        sendJson(connection, buf.GetString());
        return;
    }

    // GET /player/queue
    if (tokens.size() == 2 && tokens[0] == "player" && tokens[1] == "queue" && eqNoCase(m_method, "GET")) {
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlayerQueueJson(w);
        sendJson(connection, buf.GetString());
        return;
    }

    // POST /player/queue  { action: replace|insert, song_ids, index, play }
    // insert 的 index：插入位置，-1 或省略表示队尾；play=true 时从插入的第一首开始播。
    if (tokens.size() == 2 && tokens[0] == "player" && tokens[1] == "queue" && eqNoCase(m_method, "POST")) {
        Document doc;
        if (!parseJsonBody(connection, doc)) {
            sendJsonError(connection, Response::BAD_REQUEST, "BAD_REQUEST", "JSON body required");
            return;
        }
        vector<int> ids = parseSongIds(doc);
        if (ids.empty()) {
            sendJsonError(connection, Response::BAD_REQUEST, "BAD_REQUEST", "song_ids required");
            return;
        }
        auto extra = g_player.getMediaLibrary()->getMediaByIDs(ids);
        if (!extra || extra->getCount() == 0) {
            sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "No playable songs");
            return;
        }

        string action = "insert";
        if (doc.HasMember("action") && doc["action"].IsString()) {
            action = doc["action"].GetString();
        }

        bool play = false;
        if (doc.HasMember("play")) {
            if (doc["play"].IsBool()) play = doc["play"].GetBool();
            else if (doc["play"].IsNumber()) play = doc["play"].GetInt() != 0;
        }

        int index = -1;
        if (doc.HasMember("index") && doc["index"].IsNumber()) {
            index = doc["index"].GetInt();
        } else if (doc.HasMember("position") && doc["position"].IsString()) {
            string pos = doc["position"].GetString();
            if (pos == "next") index = g_player.getCurrentMediaIndex() + 1;
            else index = -1;
        }

        if (action == "replace") {
            g_player.setNowPlaying(extra);
            int start = index < 0 ? 0 : index;
            if (start >= (int)extra->getCount()) start = 0;
            if (play) g_player.playMedia(start);
        } else {
            auto now = g_player.getNowPlaying();
            int insertAt = index;
            if (insertAt < 0 || insertAt > (int)now->getCount()) {
                insertAt = (int)now->getCount();
            }
            now->insert(insertAt, extra->getAll());
            if (play) {
                int playAt = insertAt;
                auto first = extra->getItem(0);
                if (first) {
                    now->getItemIndex(first, playAt);
                }
                if (playAt < 0 || playAt >= (int)now->getCount()) playAt = 0;
                g_player.playMedia(playAt);
            }
        }

        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlayerStateJson(w);
        sendJson(connection, buf.GetString());
        return;
    }

    // POST /player/<command>
    // play 可带 body: { "song_ids": [...], "index": 0 } 在桌面播放器上开播。
    if (tokens.size() == 2 && tokens[0] == "player" && eqNoCase(m_method, "POST")) {
        if (tokens[1] == "play") {
            Document doc;
            if (parseJsonBody(connection, doc)) {
                vector<int> ids = parseSongIds(doc);
                if (!ids.empty()) {
                    auto playlist = g_player.getMediaLibrary()->getMediaByIDs(ids);
                    if (!playlist || playlist->getCount() == 0) {
                        sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "No playable songs");
                        return;
                    }
                    int index = 0;
                    if (doc.HasMember("index") && doc["index"].IsNumber()) {
                        index = doc["index"].GetInt();
                    }
                    if (index < 0) index = 0;
                    if (index >= (int)playlist->getCount()) index = 0;

                    g_player.setNowPlaying(playlist);
                    g_player.playMedia(index);

                    rapidjson::StringBuffer buf;
                    RapidjsonWriter w(buf);
                    writePlayerStateJson(w);
                    sendJson(connection, buf.GetString());
                    return;
                }
                if (doc.HasMember("index") && doc["index"].IsNumber()) {
                    int index = doc["index"].GetInt();
                    auto now = g_player.getNowPlaying();
                    if (now && now->getCount() > 0) {
                        if (index < 0) index = 0;
                        if (index >= (int)now->getCount()) index = 0;
                        g_player.playMedia(index);
                    }
                    rapidjson::StringBuffer buf;
                    RapidjsonWriter w(buf);
                    writePlayerStateJson(w);
                    sendJson(connection, buf.GetString());
                    return;
                }
            }
        }
        applyPlayerCommand(tokens[1].c_str(), param);
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlayerStateJson(w);
        sendJson(connection, buf.GetString());
        return;
    }

    // GET /songs/{id}
    if (tokens.size() == 2 && tokens[0] == "songs" && eqNoCase(m_method, "GET")) {
        MediaPtr media = g_player.getMediaLibrary()->getMediaByID(atoi(tokens[1].c_str()));
        if (!media) {
            sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "Song not found");
            return;
        }
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writeMediaJson(w, media.get());
        sendJson(connection, buf.GetString());
        return;
    }

    // GET /songs/{id}/stream
    if (tokens.size() == 3 && tokens[0] == "songs" && tokens[2] == "stream" && eqNoCase(m_method, "GET")) {
        sendStream(connection, atoi(tokens[1].c_str()));
        return;
    }

    // GET /songs/{id}/lyrics
    if (tokens.size() == 3 && tokens[0] == "songs" && tokens[2] == "lyrics" && eqNoCase(m_method, "GET")) {
        MediaPtr media = g_player.getMediaLibrary()->getMediaByID(atoi(tokens[1].c_str()));
        if (!media) {
            sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "Song not found");
            return;
        }
        writeLyricsJson(connection, media.get());
        return;
    }

    // GET /songs/{id}/cover
    if (tokens.size() == 3 && tokens[0] == "songs" && tokens[2] == "cover" && eqNoCase(m_method, "GET")) {
        sendCover(connection, atoi(tokens[1].c_str()));
        return;
    }

    // PUT /songs/{id}/rating
    if (tokens.size() == 3 && tokens[0] == "songs" && tokens[2] == "rating" && eqNoCase(m_method, "PUT")) {
        auto lib = g_player.getMediaLibrary();
        MediaPtr media = lib->getMediaByID(atoi(tokens[1].c_str()));
        if (!media) {
            sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "Song not found");
            return;
        }
        Document doc;
        if (!parseJsonBody(connection, doc) || !doc.HasMember("rating") || !doc["rating"].IsNumber()) {
            sendJsonError(connection, Response::BAD_REQUEST, "BAD_REQUEST", "rating is required");
            return;
        }
        double rating = doc["rating"].GetDouble();
        if (rating < 0 || rating > 5.0) {
            sendJsonError(connection, Response::BAD_REQUEST, "BAD_REQUEST", "rating must be 0.0 ~ 5.0");
            return;
        }
        uint32_t nRating = (uint32_t)std::lround(rating * 2.0) * 50; // 步长 0.5 → 0~500
        lib->rate(media.get(), nRating);
        EventStream::instance().publish("rating.changed",
            stringPrintf("{\"song_id\":%d,\"rating\":%g}", media->ID, rating));
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writeMediaJson(w, media.get());
        sendJson(connection, buf.GetString());
        return;
    }

    // GET /playlists
    if (tokens.size() == 1 && tokens[0] == "playlists" && eqNoCase(m_method, "GET")) {
        auto lib = g_player.getMediaLibrary();
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        w.StartArray();
        for (auto &brief : lib->getAllPlaylistBriefs()) {
            auto loaded = lib->getPlaylist(brief.id);
            writePlaylistBriefJson(w, brief, loaded.get());
        }
        w.EndArray();
        sendJson(connection, buf.GetString());
        return;
    }

    // POST /playlists
    if (tokens.size() == 1 && tokens[0] == "playlists" && eqNoCase(m_method, "POST")) {
        Document doc;
        string name;
        if (parseJsonBody(connection, doc) && doc.HasMember("name") && doc["name"].IsString()) {
            name = doc["name"].GetString();
        }
        if (name.empty()) {
            sendJsonError(connection, Response::BAD_REQUEST, "BAD_REQUEST", "name is required");
            return;
        }
        auto playlist = g_player.getMediaLibrary()->newPlaylist(name.c_str());
        EventStream::instance().publish("playlist.updated",
            stringPrintf("{\"playlist_id\":%d}", playlist->id));
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlaylistDetailJson(w, playlist.get());
        sendJson(connection, buf.GetString(), Response::CREATED);
        return;
    }

    // GET /playlists/{id}
    if (tokens.size() == 2 && tokens[0] == "playlists" && eqNoCase(m_method, "GET")) {
        auto playlist = g_player.getMediaLibrary()->getPlaylist(atoi(tokens[1].c_str()));
        if (!playlist) {
            sendJsonError(connection, Response::NOT_FOUND, "PLAYLIST_NOT_FOUND", "Playlist not found");
            return;
        }
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlaylistDetailJson(w, playlist.get());
        sendJson(connection, buf.GetString());
        return;
    }

    // PATCH /playlists/{id}
    if (tokens.size() == 2 && tokens[0] == "playlists" && eqNoCase(m_method, "PATCH")) {
        auto lib = g_player.getMediaLibrary();
        auto playlist = lib->getPlaylist(atoi(tokens[1].c_str()));
        if (!playlist) {
            sendJsonError(connection, Response::NOT_FOUND, "PLAYLIST_NOT_FOUND", "Playlist not found");
            return;
        }
        Document doc;
        if (parseJsonBody(connection, doc) && doc.HasMember("name") && doc["name"].IsString()) {
            playlist->name = doc["name"].GetString();
            lib->savePlaylist(playlist);
        }
        EventStream::instance().publish("playlist.updated",
            stringPrintf("{\"playlist_id\":%d}", playlist->id));
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlaylistDetailJson(w, playlist.get());
        sendJson(connection, buf.GetString());
        return;
    }

    // DELETE /playlists/{id}
    if (tokens.size() == 2 && tokens[0] == "playlists" && eqNoCase(m_method, "DELETE")) {
        auto lib = g_player.getMediaLibrary();
        int id = atoi(tokens[1].c_str());
        if (!lib->getPlaylist(id)) {
            sendJsonError(connection, Response::NOT_FOUND, "PLAYLIST_NOT_FOUND", "Playlist not found");
            return;
        }
        lib->deltePlaylist(id);
        EventStream::instance().publish("playlist.updated", "{\"playlist_id\":null}");
        sendNoContent(connection);
        return;
    }

    // POST /playlists/{id}/songs
    if (tokens.size() == 3 && tokens[0] == "playlists" && tokens[2] == "songs" && eqNoCase(m_method, "POST")) {
        auto lib = g_player.getMediaLibrary();
        auto playlist = lib->getPlaylist(atoi(tokens[1].c_str()));
        if (!playlist) {
            sendJsonError(connection, Response::NOT_FOUND, "PLAYLIST_NOT_FOUND", "Playlist not found");
            return;
        }
        Document doc;
        vector<int> ids;
        if (parseJsonBody(connection, doc)) {
            ids = parseSongIds(doc);
        }
        if (ids.empty()) {
            sendJsonError(connection, Response::BAD_REQUEST, "BAD_REQUEST", "song_ids is required");
            return;
        }
        auto extra = lib->getMediaByIDs(ids);
        if (extra) {
            lib->addToPlaylist(playlist->id, extra);
            playlist = lib->getPlaylist(playlist->id);
        }
        EventStream::instance().publish("playlist.updated",
            stringPrintf("{\"playlist_id\":%d}", playlist->id));
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlaylistDetailJson(w, playlist.get());
        sendJson(connection, buf.GetString());
        return;
    }

    // DELETE /playlists/{id}/songs/{song_id}
    if (tokens.size() == 4 && tokens[0] == "playlists" && tokens[2] == "songs" && eqNoCase(m_method, "DELETE")) {
        auto lib = g_player.getMediaLibrary();
        auto playlist = lib->getPlaylist(atoi(tokens[1].c_str()));
        if (!playlist) {
            sendJsonError(connection, Response::NOT_FOUND, "PLAYLIST_NOT_FOUND", "Playlist not found");
            return;
        }
        int songId = atoi(tokens[3].c_str());
        int index = -1;
        auto media = playlist->getItemByID(songId, &index);
        if (!media || index < 0) {
            sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "Song not in playlist");
            return;
        }
        playlist->removeItem(index);
        lib->savePlaylist(playlist);
        EventStream::instance().publish("playlist.updated",
            stringPrintf("{\"playlist_id\":%d}", playlist->id));
        sendNoContent(connection);
        return;
    }

    // PUT /playlists/{id}/songs/order
    if (tokens.size() == 4 && tokens[0] == "playlists" && tokens[2] == "songs" && tokens[3] == "order"
        && eqNoCase(m_method, "PUT")) {
        auto lib = g_player.getMediaLibrary();
        auto playlist = lib->getPlaylist(atoi(tokens[1].c_str()));
        if (!playlist) {
            sendJsonError(connection, Response::NOT_FOUND, "PLAYLIST_NOT_FOUND", "Playlist not found");
            return;
        }
        Document doc;
        vector<int> ids;
        if (parseJsonBody(connection, doc)) {
            ids = parseSongIds(doc);
        }
        auto ordered = lib->getMediaByIDs(ids);
        playlist->clear();
        if (ordered) {
            playlist->insert(-1, ordered->getAll());
        }
        lib->savePlaylist(playlist);
        EventStream::instance().publish("playlist.updated",
            stringPrintf("{\"playlist_id\":%d}", playlist->id));
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        writePlaylistDetailJson(w, playlist.get());
        sendJson(connection, buf.GetString());
        return;
    }

    // POST /history
    if (tokens.size() == 1 && tokens[0] == "history" && eqNoCase(m_method, "POST")) {
        Document doc;
        if (!parseJsonBody(connection, doc) || !doc.HasMember("song_id") || !doc["song_id"].IsNumber()) {
            sendJsonError(connection, Response::BAD_REQUEST, "BAD_REQUEST", "song_id is required");
            return;
        }
        int songId = doc["song_id"].IsInt() ? doc["song_id"].GetInt() : (int)doc["song_id"].GetInt64();
        auto lib = g_player.getMediaLibrary();
        if (!lib->getMediaByID(songId)) {
            sendJsonError(connection, Response::NOT_FOUND, "SONG_NOT_FOUND", "Song not found");
            return;
        }
        string playedAt;
        if (doc.HasMember("played_at") && doc["played_at"].IsString()) {
            playedAt = doc["played_at"].GetString();
        }
        lib->addPlayHistory(songId, playedAt.c_str());
        EventStream::instance().publish("history.updated", stringPrintf("{\"song_id\":%d}", songId));
        sendJson(connection, "{}", Response::CREATED);
        return;
    }

    // GET /history/recent
    if (tokens.size() == 2 && tokens[0] == "history" && tokens[1] == "recent" && eqNoCase(m_method, "GET")) {
        int days = queryInt(connection->request().uri, "days", 30);
        auto daysData = g_player.getMediaLibrary()->getRecentPlayHistory(days);
        rapidjson::StringBuffer buf;
        RapidjsonWriter w(buf);
        w.StartObject();
        w.Key("days");
        w.StartArray();
        for (auto &day : daysData) {
            w.StartObject();
            w.Key("date"); w.String(day.date.c_str());
            w.Key("items");
            w.StartArray();
            for (auto &item : day.items) {
                w.StartObject();
                w.Key("song_id"); w.Int(item.songId);
                w.Key("count"); w.Int(item.count);
                w.Key("last_played_at"); w.String(item.lastPlayedAt.c_str());
                w.EndObject();
            }
            w.EndArray();
            w.EndObject();
        }
        w.EndArray();
        w.EndObject();
        sendJson(connection, buf.GetString());
        return;
    }

    sendJsonError(connection, Response::NOT_FOUND, "NOT_FOUND", "Not found");
}

} // namespace HttpServer