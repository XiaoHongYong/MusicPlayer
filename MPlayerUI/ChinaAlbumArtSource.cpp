#include "../Utils/Utils.h"
#include "ChinaAlbumArtSource.h"
#include "../Utils/HttpsGet.h"
#include "../Utils/rapidjson.h"
#include "../ImageLib/RawImageData.h"

#include <algorithm>


static cstr_t chinaBrowserUA() {
    return "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
}

static string jsonString(const rapidjson::Value &obj, const char *key) {
    return getMemberString(obj, key, "");
}

static int matchScore(const MediaIdentity &query, const string &artist, const string &album, const string &title, int base) {
    int score = base;
    if (!query.album.empty() && !album.empty() && strIsISame(query.album.c_str(), album.c_str())) {
        score += 30;
    }
    if (!query.artist.empty() && !artist.empty() && strIsISame(query.artist.c_str(), artist.c_str())) {
        score += 20;
    }
    if (!query.title.empty() && !title.empty() && strIsISame(query.title.c_str(), title.c_str())) {
        score += 10;
    }
    return score;
}

static string toHttps(string url) {
    if (iStartsWith(url.c_str(), "http://")) {
        url.replace(0, 4, "https");
    }
    return url;
}

static void addImageResult(VecAlbumArtSearchResults &results, const string &imageUrl, int score, const string &sourceName) {
    if (imageUrl.empty()) {
        return;
    }
    string url = toHttps(imageUrl);
    for (auto &exist : results) {
        if (exist.imageUrl == url) {
            if (score > exist.score) {
                exist.score = score;
            }
            return;
        }
    }
    AlbumArtSearchResult item;
    item.score = score;
    item.imageUrl = url;
    item.sourceName = sourceName;
    results.push_back(item);
}

static string enlargeNeteasePic(string url) {
    url = toHttps(url);
    if (url.empty()) {
        return url;
    }
    if (url.find("param=") == string::npos) {
        url += (url.find('?') == string::npos) ? "?param=500y500" : "&param=500y500";
    }
    return url;
}

static string enlargeITunesArtwork(string url) {
    strrep(url, "100x100bb", "600x600bb");
    strrep(url, "60x60bb", "600x600bb");
    return toHttps(url);
}

static string jsonPicUrl(const rapidjson::Value &obj) {
    string pic = jsonString(obj, "picUrl");
    if (pic.empty()) {
        pic = jsonString(obj, "blurPicUrl");
    }
    return pic;
}

static string firstNamedArtist(const rapidjson::Value &obj, const char *key) {
    auto &arr = getMember(obj, key);
    if (arr.IsArray() && arr.Size() > 0 && arr[0].IsObject()) {
        return jsonString(arr[0], "name");
    }
    return "";
}

static string neteaseArtistName(const rapidjson::Value &item) {
    auto &artistObj = getMember(item, "artist");
    if (artistObj.IsObject()) {
        string name = jsonString(artistObj, "name");
        if (!name.empty()) {
            return name;
        }
    }
    string name = firstNamedArtist(item, "artists");
    if (!name.empty()) {
        return name;
    }
    return firstNamedArtist(item, "ar");
}

static void neteaseAlbumFields(const rapidjson::Value &item, string &album, string &pic) {
    auto takeAlbumObj = [&](const char *key) {
        auto &obj = getMember(item, key);
        if (!obj.IsObject()) {
            return;
        }
        if (album.empty()) {
            album = jsonString(obj, "name");
        }
        if (pic.empty()) {
            pic = jsonPicUrl(obj);
        }
    };
    takeAlbumObj("album");
    takeAlbumObj("al");
}

CChinaAlbumArtSource::CChinaAlbumArtSource() {
}

CChinaAlbumArtSource::~CChinaAlbumArtSource() {
}

cstr_t CChinaAlbumArtSource::name() const {
    return "China";
}

string CChinaAlbumArtSource::buildKeyword(const MediaIdentity &query) {
    string k;
    if (!query.artist.empty() && !query.album.empty()) {
        k = query.artist + " " + query.album;
    } else if (!query.album.empty()) {
        k = query.album;
    } else if (!query.artist.empty() && !query.title.empty()) {
        k = query.artist + " " + query.title;
    } else if (!query.title.empty()) {
        k = query.title;
    } else {
        k = query.artist;
    }
    trimStr(k);
    return k;
}

void CChinaAlbumArtSource::waitForRateLimit() {
    int64_t now = getTickCount();
    int64_t elapsed = now - m_lastRequestMs;
    const int64_t minInterval = 1100;
    if (m_lastRequestMs > 0 && elapsed < minInterval) {
        Sleep((uint32_t)(minInterval - elapsed));
    }
    m_lastRequestMs = getTickCount();
}

bool CChinaAlbumArtSource::httpGet(cstr_t url, int &httpCode, string &body, cstr_t referer) {
    waitForRateLimit();
    int nRet = httpGetUrl(url, chinaBrowserUA(), httpCode, body, referer);
    if (nRet != ERR_OK) {
        m_lastError = stringPrintf("%s (%s)", (cstr_t)Error2Str(nRet), url);
        ERR_LOG2("AlbumArt China HTTP failed: %s, err=%d", url, nRet);
        return false;
    }
    if (httpCode != 200) {
        m_lastError = stringPrintf("HTTP %d (%s)", httpCode, url);
        return false;
    }
    m_lastError.clear();
    return true;
}

bool CChinaAlbumArtSource::parseNeteaseSearch(const string &body, const MediaIdentity &query, int type, VecAlbumArtSearchResults &results) {
    rapidjson::Document doc;
    doc.Parse(body.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        m_lastError = "Netease: invalid JSON";
        return false;
    }
    if (getMemberInt(doc, "code", 0) != 200) {
        m_lastError = stringPrintf("Netease API code %d", getMemberInt(doc, "code", 0));
        return false;
    }

    auto &result = getMember(doc, "result");
    if (!result.IsObject()) {
        return false;
    }

    size_t before = results.size();
    if (type == 10) {
        auto &albums = getMember(result, "albums");
        if (!albums.IsArray()) {
            return false;
        }
        int index = 0;
        for (auto it = albums.Begin(); it != albums.End(); ++it, ++index) {
            if (!it->IsObject()) {
                continue;
            }
            string album = jsonString(*it, "name");
            string pic = jsonPicUrl(*it);
            string artist = neteaseArtistName(*it);
            int score = matchScore(query, artist, album, "", 80 - index * 5);
            addImageResult(results, enlargeNeteasePic(pic), score, "Netease");
        }
    } else {
        auto &songs = getMember(result, "songs");
        if (!songs.IsArray()) {
            return false;
        }
        int index = 0;
        for (auto it = songs.Begin(); it != songs.End(); ++it, ++index) {
            if (!it->IsObject()) {
                continue;
            }
            string title = jsonString(*it, "name");
            string artist = neteaseArtistName(*it);
            string album, pic;
            neteaseAlbumFields(*it, album, pic);
            int score = matchScore(query, artist, album, title, 70 - index * 5);
            addImageResult(results, enlargeNeteasePic(pic), score, "Netease");
        }
    }
    m_lastError.clear();
    return results.size() > before;
}

bool CChinaAlbumArtSource::searchNetease(const MediaIdentity &query, int type, VecAlbumArtSearchResults &results) {
    string keyword = buildKeyword(query);
    if (keyword.empty()) {
        return false;
    }

    // type: 10=专辑, 1=单曲。cloudsearch 单曲仍带 al.picUrl；旧 /api/search/get/web 单曲只剩 picId。
    string quoted = uriQuote(keyword.c_str());
    string typeStr = std::to_string(type);
    const string urls[] = {
        string("https://music.163.com/api/cloudsearch/pc?offset=0&limit=8&type=") + typeStr + "&s=" + quoted,
        string("https://music.163.com/api/search/get/web?csrf_token=&offset=0&limit=8&type=") + typeStr + "&s=" + quoted,
    };

    size_t before = results.size();
    for (auto &url : urls) {
        int httpCode = 0;
        string body;
        if (!httpGet(url.c_str(), httpCode, body, "https://music.163.com/")) {
            continue;
        }
        if (parseNeteaseSearch(body, query, type, results) && results.size() > before) {
            return true;
        }
        // 合法 JSON 但没有封面：不必再打旧接口。解析失败则试下一个 URL。
        if (startsWith(m_lastError.c_str(), "Netease")) {
            continue;
        }
        break;
    }
    return results.size() > before;
}

bool CChinaAlbumArtSource::searchITunes(const MediaIdentity &query, const string &entity, VecAlbumArtSearchResults &results) {
    string keyword = buildKeyword(query);
    if (keyword.empty()) {
        return false;
    }

    // iTunes 中国区目录已空；港台仍能搜到华语，再回退美区。
    static const char *kCountries[] = { "hk", "tw", "us" };
    size_t beforeAll = results.size();
    for (cstr_t country : kCountries) {
        string url = string("https://itunes.apple.com/search?media=music&limit=8&country=")
            + country + "&entity=" + entity + "&term=" + uriQuote(keyword.c_str());

        int httpCode = 0;
        string body;
        if (!httpGet(url.c_str(), httpCode, body, nullptr)) {
            continue;
        }

        rapidjson::Document doc;
        doc.Parse(body.c_str());
        if (doc.HasParseError() || !doc.IsObject()) {
            m_lastError = stringPrintf("iTunes %s: invalid JSON", country);
            continue;
        }
        auto &arr = getMember(doc, "results");
        if (!arr.IsArray()) {
            continue;
        }

        size_t before = results.size();
        int index = 0;
        for (auto it = arr.Begin(); it != arr.End(); ++it, ++index) {
            if (!it->IsObject()) {
                continue;
            }
            string artist = jsonString(*it, "artistName");
            string album = jsonString(*it, "collectionName");
            string title = jsonString(*it, "trackName");
            string pic = jsonString(*it, "artworkUrl100");
            if (pic.empty()) {
                pic = jsonString(*it, "artworkUrl60");
            }
            int score = matchScore(query, artist, album, title, 75 - index * 5);
            addImageResult(results, enlargeITunesArtwork(pic), score, "iTunes");
        }
        if (results.size() > before) {
            m_lastError.clear();
            return true;
        }
    }
    return results.size() > beforeAll;
}

bool CChinaAlbumArtSource::search(const MediaIdentity &query, VecAlbumArtSearchResults &results) {
    results.clear();
    if (buildKeyword(query).empty()) {
        return false;
    }

    if (!query.album.empty()) {
        searchNetease(query, 10, results);
    }
    if (results.empty()) {
        searchNetease(query, 1, results);
    }
    if (results.empty()) {
        searchITunes(query, query.album.empty() ? "song" : "album", results);
    }
    if (results.empty() && !query.album.empty()) {
        searchITunes(query, "song", results);
    }

    std::sort(results.begin(), results.end(), [](const AlbumArtSearchResult &a, const AlbumArtSearchResult &b) {
        return a.score > b.score;
    });
    return !results.empty();
}

bool CChinaAlbumArtSource::downloadImage(const AlbumArtSearchResult &item, string &imageData, string &ext) {
    imageData.clear();
    ext = ".jpg";
    if (item.imageUrl.empty()) {
        return false;
    }

    cstr_t referer = nullptr;
    if (item.sourceName == "Netease") {
        referer = "https://music.163.com/";
    }

    int httpCode = 0;
    if (!httpGet(item.imageUrl.c_str(), httpCode, imageData, referer)) {
        return false;
    }
    if (imageData.size() < 32) {
        m_lastError = stringPrintf("%s: empty image (%s)", item.sourceName.c_str(), item.imageUrl.c_str());
        imageData.clear();
        return false;
    }

    ext = guessPictureDataExt(StringView(imageData.c_str(), imageData.size()));
    if (strcmp(ext.c_str(), ".err") == 0) {
        m_lastError = stringPrintf("%s: not a picture (%s)", item.sourceName.c_str(), item.imageUrl.c_str());
        imageData.clear();
        return false;
    }
    return true;
}

IAlbumArtSource *createChinaAlbumArtSource() {
    return new CChinaAlbumArtSource();
}

#if UNIT_TEST

#include "utils/unittest.h"

TEST(ChinaAlbumArtSource, buildKeyword) {
    ASSERT_EQ(CChinaAlbumArtSource::buildKeyword({ "周杰伦", "范特西", "爱在西元前" }), "周杰伦 范特西");
    ASSERT_EQ(CChinaAlbumArtSource::buildKeyword({ "周杰伦", "", "稻香" }), "周杰伦 稻香");
}

class TestChinaAlbumArtSource : public CChinaAlbumArtSource {
public:
    using CChinaAlbumArtSource::parseNeteaseSearch;
};

TEST(ChinaAlbumArtSource, parseSongPicUrlFromCloudSearchAndLegacy) {
    TestChinaAlbumArtSource src;
    MediaIdentity q{ "周杰伦", "", "稻香" };

    VecAlbumArtSearchResults legacy;
    ASSERT_FALSE(src.parseNeteaseSearch(
        "{\"code\":200,\"result\":{\"songs\":[{\"name\":\"稻香\",\"artists\":[{\"name\":\"周杰伦\"}],"
        "\"album\":{\"name\":\"魔杰座\",\"id\":1,\"picId\":123}}]}}",
        q, 1, legacy));
    ASSERT_TRUE(legacy.empty());

    VecAlbumArtSearchResults cloud;
    ASSERT_TRUE(src.parseNeteaseSearch(
        "{\"code\":200,\"result\":{\"songs\":[{\"name\":\"稻香\",\"ar\":[{\"name\":\"周杰伦\"}],"
        "\"al\":{\"name\":\"魔杰座\",\"picUrl\":\"http://p2.music.126.net/cover.jpg\"}}]}}",
        q, 1, cloud));
    ASSERT_EQ((int)cloud.size(), 1);
    ASSERT_TRUE(cloud[0].imageUrl.find("cover.jpg") != string::npos);
}

#endif
