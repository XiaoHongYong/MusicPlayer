//
//  PlayerEventSender.cpp
//  MusicPlayer
//

#include "PlayerEventSender.hpp"
#include "Http/EventStream.hpp"
#include "Utils/rapidjson.h"
#include "MPlayer/Player.h"


cstr_t loopModeToString(int loop) {
    switch (loop) {
        case MP_LOOP_OFF: return "off";
        case MP_LOOP_ALL: return "all";
        case MP_LOOP_TRACK: return "one";
        default: assert(0);
    }
    return "";
}

LoopMode loopModeFromString(cstr_t loop) {
    if (strcmp(loop, "off") == 0) return MP_LOOP_OFF;
    else if (strcmp(loop, "all") == 0) return MP_LOOP_ALL;
    else if (strcmp(loop, "one") == 0) return MP_LOOP_TRACK;
    return MP_LOOP_ALL;
}

static std::string jsonPlayerState() {
    MediaPtr cur = g_player.getCurrentMedia();
    rapidjson::StringBuffer buf;
    RapidjsonWriter w(buf);
    w.StartObject();
    w.Key("state");
    switch (g_player.getPlayerState()) {
        case PS_STOPPED: w.String("idle"); break;
        case PS_PAUSED:  w.String("paused"); break;
        case PS_PLAYING: w.String("playing"); break;
        default: w.String("idle"); break;
    }
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
    w.Key("state_version"); w.Int64(HttpServer::EventStream::instance().stateVersion() + 1);
    w.EndObject();
    return buf.GetString();
}

static std::string jsonPlayerQueue() {
    auto playlist = g_player.getNowPlaying();
    rapidjson::StringBuffer buf;
    RapidjsonWriter w(buf);
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
        w.StartObject();
        w.Key("id"); w.Int(media->ID);
        w.Key("artist"); w.String(media->artist.c_str());
        w.Key("album"); w.String(media->album.c_str());
        w.Key("title"); w.String(media->title.c_str());
        w.Key("year"); w.Int(media->year);
        w.Key("genre"); w.String(media->genre.c_str());
        w.Key("url"); w.String(media->url.c_str());
        w.Key("duration"); w.Int(media->duration > 0 ? (media->duration + 500) / 1000 : 0);
        w.Key("fileSize"); w.Int64(media->fileSize);
        w.Key("timeAdded"); w.Int64(media->timeAdded);
        w.Key("timePlayed"); w.Int64(media->timePlayed);
        w.Key("lyricsFile"); w.String(media->lyricsFile.c_str());
        w.Key("rating"); w.Double(media->rating / 100.0);
        w.Key("format"); w.String(media->format.c_str());
        w.Key("play_count"); w.Int(media->countPlayed);
        w.Key("has_lyrics"); w.Bool(!media->lyricsFile.empty());
        w.Key("bitRate"); w.Int(media->bitRate);
        w.Key("channels"); w.Int(media->channels);
        w.Key("bitsPerSample"); w.Int(media->bitsPerSample);
        w.Key("sampleRate"); w.Int(media->sampleRate);
        w.EndObject();
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();
    return buf.GetString();
}

PlayerEventSender::PlayerEventSender() {
    static EventType events[] = {
        ET_PLAYER_STATUS_CHANGED,
        ET_PLAYER_SEEK,
        ET_PLAYER_CUR_MEDIA_CHANGED,
        ET_PLAYER_CUR_MEDIA_INFO_CHANGED,
        ET_PLAYER_CUR_PLAYLIST_CHANGED,
        ET_PLAYER_SETTING_CHANGED,
        ET_PLAY_HISTORY_RECORDED,
    };

    for (auto event : events) {
        registerHandler(MPlayerApp::getEventsDispatcher(), event);
    }
}

void PlayerEventSender::onEvent(const IEvent *event) {
    auto &hub = HttpServer::EventStream::instance();
    switch (event->eventType) {
        case ET_PLAYER_STATUS_CHANGED:
        case ET_PLAYER_SEEK:
        case ET_PLAYER_POS_UPDATE:
        case ET_PLAYER_SETTING_CHANGED:
            hub.publish("player.state_changed", jsonPlayerState());
            break;
        case ET_PLAYER_CUR_MEDIA_INFO_CHANGED:
        case ET_PLAYER_CUR_MEDIA_CHANGED:
            hub.publish("player.song_changed", jsonPlayerState());
            break;
        case ET_PLAYER_CUR_PLAYLIST_CHANGED:
            hub.publish("player.queue_changed", jsonPlayerQueue());
            break;
        case ET_PLAY_HISTORY_RECORDED: {
            auto *hist = (CEventPlayHistoryRecorded *)event;
            hub.publish("history.updated", stringPrintf("{\"song_id\":%d}", hist->songId));
            break;
        }
        default:
            break;
    }
}
