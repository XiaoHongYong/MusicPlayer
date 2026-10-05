#include "../Utils/Utils.h"
#include "MusicBrainzAlbumArtSource.h"
#include "../Utils/HttpsGet.h"
#include "../Utils/rapidjson.h"
#include "../ImageLib/RawImageData.h"
#include "../version.h"

#include <algorithm>


static cstr_t albumArtUserAgent() {
    static string ua;
    if (ua.empty()) {
        ua = string("MusicPlayer/") + VERSION_STR + " ( https://www.crintsoft.com/music-player )";
    }
    return ua.c_str();
}

CMusicBrainzAlbumArtSource::CMusicBrainzAlbumArtSource() {
}

CMusicBrainzAlbumArtSource::~CMusicBrainzAlbumArtSource() {
}

cstr_t CMusicBrainzAlbumArtSource::name() const {
    return "MusicBrainz";
}

string CMusicBrainzAlbumArtSource::luceneEscape(const string &text) {
    string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if (strchr("+-&|!(){}[]^\"~*?:\\/", c)) {
            out += '\\';
        }
        out += (char)c;
    }
    return out;
}

static string luceneQuoted(const string &text) {
    return string("\"") + CMusicBrainzAlbumArtSource::luceneEscape(text) + "\"";
}

string CMusicBrainzAlbumArtSource::buildReleaseQuery(const MediaIdentity &query) {
    string q;
    if (!query.album.empty()) {
        q += "release:" + luceneQuoted(query.album);
    }
    if (!query.artist.empty()) {
        if (!q.empty()) {
            q += " AND ";
        }
        q += "artist:" + luceneQuoted(query.artist);
    }
    return q;
}

string CMusicBrainzAlbumArtSource::buildRecordingQuery(const MediaIdentity &query) {
    string q;
    if (!query.title.empty()) {
        q += "recording:" + luceneQuoted(query.title);
    }
    if (!query.artist.empty()) {
        if (!q.empty()) {
            q += " AND ";
        }
        q += "artist:" + luceneQuoted(query.artist);
    }
    return q;
}

void CMusicBrainzAlbumArtSource::waitForRateLimit() {
    int64_t now = getTickCount();
    int64_t elapsed = now - m_lastRequestMs;
    const int64_t minInterval = 1100;
    if (m_lastRequestMs > 0 && elapsed < minInterval) {
        Sleep((uint32_t)(minInterval - elapsed));
    }
    m_lastRequestMs = getTickCount();
}

bool CMusicBrainzAlbumArtSource::httpGet(cstr_t url, int &httpCode, string &body) {
    waitForRateLimit();
    int nRet = httpGetUrl(url, albumArtUserAgent(), httpCode, body);
    if (nRet != ERR_OK) {
        m_lastError = stringPrintf("%s (%s)", (cstr_t)Error2Str(nRet), url);
        ERR_LOG2("AlbumArt HTTP failed: %s, err=%d", url, nRet);
        return false;
    }
    if (httpCode != 200) {
        m_lastError = stringPrintf("HTTP %d (%s)", httpCode, url);
        return false;
    }
    m_lastError.clear();
    return true;
}

static string jsonString(const rapidjson::Value &obj, const char *key) {
    return getMemberString(obj, key, "");
}

static int jsonInt(const rapidjson::Value &obj, const char *key) {
    return getMemberInt(obj, key, 0);
}

static string firstArtistName(const rapidjson::Value &obj) {
    auto &credit = getMember(obj, "artist-credit");
    if (credit.IsArray() && credit.Size() > 0 && credit[0].IsObject()) {
        string name = jsonString(credit[0], "name");
        if (!name.empty()) {
            return name;
        }
        auto &artist = getMember(credit[0], "artist");
        if (artist.IsObject()) {
            return jsonString(artist, "name");
        }
    }
    return "";
}

static void addReleaseResult(VecAlbumArtSearchResults &results, const string &releaseId, int score, const string &source) {
    if (releaseId.empty()) {
        return;
    }
    for (auto &exist : results) {
        if (exist.releaseId == releaseId) {
            if (score > exist.score) {
                exist.score = score;
            }
            return;
        }
    }
    AlbumArtSearchResult item;
    item.score = score;
    item.releaseId = releaseId;
    item.imageUrl = string("https://coverartarchive.org/release/") + releaseId + "/front-500";
    item.sourceName = source;
    results.push_back(item);
}

static int refineScore(int mbScore, const MediaIdentity &query, const string &releaseTitle, const string &artistName) {
    int score = mbScore;
    if (!query.album.empty() && !releaseTitle.empty()
        && strIsISame(query.album.c_str(), releaseTitle.c_str())) {
        score += 30;
    }
    if (!query.artist.empty() && !artistName.empty()
        && strIsISame(query.artist.c_str(), artistName.c_str())) {
        score += 20;
    }
    return score;
}

bool CMusicBrainzAlbumArtSource::searchReleases(const MediaIdentity &query, VecAlbumArtSearchResults &results) {
    string lucene = buildReleaseQuery(query);
    if (lucene.empty()) {
        return false;
    }

    string url = "https://musicbrainz.org/ws/2/release/?query=" + uriQuote(lucene.c_str())
        + "&fmt=json&limit=8";

    int httpCode = 0;
    string body;
    if (!httpGet(url.c_str(), httpCode, body)) {
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(body.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        m_lastError = "MusicBrainz: invalid JSON";
        return false;
    }

    auto &releases = getMember(doc, "releases");
    if (!releases.IsArray()) {
        return false;
    }

    for (auto it = releases.Begin(); it != releases.End(); ++it) {
        if (!it->IsObject()) {
            continue;
        }
        string id = jsonString(*it, "id");
        string title = jsonString(*it, "title");
        string artistName = firstArtistName(*it);
        int score = refineScore(jsonInt(*it, "score"), query, title, artistName);
        addReleaseResult(results, id, score, name());
    }
    return !results.empty();
}

bool CMusicBrainzAlbumArtSource::searchRecordings(const MediaIdentity &query, VecAlbumArtSearchResults &results) {
    string lucene = buildRecordingQuery(query);
    if (lucene.empty()) {
        return false;
    }

    string url = "https://musicbrainz.org/ws/2/recording/?query=" + uriQuote(lucene.c_str())
        + "&fmt=json&limit=8";

    int httpCode = 0;
    string body;
    if (!httpGet(url.c_str(), httpCode, body)) {
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(body.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        m_lastError = "MusicBrainz: invalid JSON";
        return false;
    }

    auto &recordings = getMember(doc, "recordings");
    if (!recordings.IsArray()) {
        return false;
    }

    for (auto rec = recordings.Begin(); rec != recordings.End(); ++rec) {
        if (!rec->IsObject()) {
            continue;
        }
        int recScore = jsonInt(*rec, "score");
        string artistName = firstArtistName(*rec);
        auto &releases = getMember(*rec, "releases");
        if (!releases.IsArray()) {
            continue;
        }
        int index = 0;
        for (auto rel = releases.Begin(); rel != releases.End(); ++rel, ++index) {
            if (!rel->IsObject()) {
                continue;
            }
            string id = jsonString(*rel, "id");
            string title = jsonString(*rel, "title");
            int score = refineScore(recScore - index, query, title, artistName);
            addReleaseResult(results, id, score, name());
        }
    }
    return !results.empty();
}

bool CMusicBrainzAlbumArtSource::search(const MediaIdentity &query, VecAlbumArtSearchResults &results) {
    results.clear();
    if (mediaIdentityIsEmpty(query)) {
        return false;
    }

    if (!query.album.empty()) {
        searchReleases(query, results);
    }
    if (results.empty() && !query.title.empty()) {
        searchRecordings(query, results);
    }

    std::sort(results.begin(), results.end(), [](const AlbumArtSearchResult &a, const AlbumArtSearchResult &b) {
        return a.score > b.score;
    });
    return !results.empty();
}

bool CMusicBrainzAlbumArtSource::downloadImage(const AlbumArtSearchResult &item, string &imageData, string &ext) {
    imageData.clear();
    ext = ".jpg";
    if (item.imageUrl.empty()) {
        return false;
    }

    int httpCode = 0;
    if (!httpGet(item.imageUrl.c_str(), httpCode, imageData)) {
        return false;
    }
    if (imageData.size() < 32) {
        m_lastError = stringPrintf("Cover Art Archive: empty image (%s)", item.imageUrl.c_str());
        imageData.clear();
        return false;
    }

    ext = guessPictureDataExt(StringView(imageData.c_str(), imageData.size()));
    if (strcmp(ext.c_str(), ".err") == 0) {
        m_lastError = stringPrintf("Cover Art Archive: not a picture (%s)", item.imageUrl.c_str());
        imageData.clear();
        return false;
    }
    return true;
}

IAlbumArtSource *createDefaultAlbumArtSource() {
    return new CMusicBrainzAlbumArtSource();
}

#if UNIT_TEST

#include "utils/unittest.h"

TEST(MusicBrainzAlbumArtSource, luceneEscape) {
    ASSERT_EQ(CMusicBrainzAlbumArtSource::luceneEscape("AC/DC"), "AC\\/DC");
    string q = CMusicBrainzAlbumArtSource::buildReleaseQuery({ "Pink Floyd", "The Wall", "Hey You" });
    ASSERT_TRUE(q.find("release:\"The Wall\"") != string::npos);
    ASSERT_TRUE(q.find("artist:\"Pink Floyd\"") != string::npos);
}

#endif
