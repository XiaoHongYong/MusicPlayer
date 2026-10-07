#include "MetadataInference.h"
#include "FilenameParser.h"
#include "NumberParser.h"
#include "Normalizer.h"
#include "Utils/Utils.h"

// 单测：覆盖 docs/web-console/metadata.md §6~§13 的主要样例。

#if UNIT_TEST

#include "utils/unittest.h"

using namespace MetadataInference;

TEST(MetadataInference, Filename_Artist_Title) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("Jay Chou - Qing Tian");
    EXPECT_EQ(r.artist, "Jay Chou");
    EXPECT_EQ(r.title, "Qing Tian");
    EXPECT_TRUE(r.hasSeparator);
}

TEST(MetadataInference, Filename_Chinese_Artist_Title) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("周杰伦 - 七里香");
    EXPECT_EQ(r.artist, "周杰伦");
    EXPECT_EQ(r.title, "七里香");
}

TEST(MetadataInference, Filename_Track_Title) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("01 - My Song");
    EXPECT_EQ(r.track, 1);
    EXPECT_TRUE(r.artist.empty());
    EXPECT_EQ(r.title, "My Song");
}

TEST(MetadataInference, Filename_TrackDot_Title) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("01. 夜曲");
    EXPECT_EQ(r.track, 1);
    EXPECT_TRUE(r.artist.empty());
    EXPECT_EQ(r.title, "夜曲");
}

TEST(MetadataInference, Filename_DiscTrack) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("1-01 Speaker to Me");
    EXPECT_EQ(r.disc, 1);
    EXPECT_EQ(r.track, 1);
    EXPECT_EQ(r.title, "Speaker to Me");
}

TEST(MetadataInference, Filename_DiscText) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("Disc 1 Track 2 - Floor Fingers");
    EXPECT_EQ(r.disc, 1);
    EXPECT_EQ(r.track, 2);
    EXPECT_EQ(r.title, "Floor Fingers");
}

TEST(MetadataInference, Filename_GarbageTokens) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("01 - 周杰伦 - 我的地盘 [320K][WEB-DL] (Official Audio)");
    EXPECT_EQ(r.track, 1);
    EXPECT_EQ(r.artist, "周杰伦");
    EXPECT_EQ(r.title, "我的地盘");
    EXPECT_TRUE(r.version.empty());
}

TEST(MetadataInference, Filename_Version_Live) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("Coldplay - Yellow (Live)");
    EXPECT_EQ(r.artist, "Coldplay");
    EXPECT_EQ(r.title, "Yellow");
    EXPECT_EQ(r.version, "Live");
}

TEST(MetadataInference, Filename_Version_RadioEdit) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("Get Lucky (Radio Edit)");
    EXPECT_EQ(r.title, "Get Lucky");
    EXPECT_EQ(r.version, "Radio Edit");
}

TEST(MetadataInference, Filename_YearTrailing) {
    InferenceEngine e;
    NumberParser np;
    FilenameParser fp(e.garbage(), np);
    auto r = fp.parse("我的地盘 (2004)");
    EXPECT_EQ(r.year, 2004);
    EXPECT_EQ(r.title, "我的地盘");
}

TEST(MetadataInference, Path_ArtistAlbum) {
    InferenceEngine e;
    auto r = e.infer(InferInput{
        "/Music/周杰伦/七里香/01 - 我的地盘.mp3",
        "01 - 我的地盘", "", "", "", "", 0, -1
    });
    EXPECT_EQ(r.artist.value, "周杰伦");
    EXPECT_EQ(r.album.value, "七里香");
    EXPECT_EQ(r.title.value, "我的地盘");
    EXPECT_EQ(r.trackNumber.value, "1");
    EXPECT_EQ(r.status, MetadataStatus::NORMALIZED);
    EXPECT_EQ(r.artist.source, MetadataSourceType::Path);
}

TEST(MetadataInference, Path_ArtistAlbum_WithYearDir) {
    InferenceEngine e;
    auto r = e.infer(InferInput{
        "/Music/Pink Floyd/1973 - The Dark Side of the Moon/01 - Speak to Me.flac",
        "01 - Speak to Me", "Pink Floyd", "Speak to Me", "", "", 1973, -1
    });
    EXPECT_EQ(r.artist.value, "Pink Floyd");
    EXPECT_EQ(r.title.value, "Speak to Me");
    EXPECT_FALSE(r.album.value.empty());
    EXPECT_EQ(r.year.value, "1973");
}

TEST(MetadataInference, Tag_Garbage_Overridden_By_Path) {
    InferenceEngine e;
    auto r = e.infer(InferInput{
        "/Music/周杰伦/七里香/01 - 我的地盘.mp3",
        "01 - 我的地盘", "Unknown Artist", "Track 01", "Unknown", "", 0, -1
    });
    EXPECT_EQ(r.artist.value, "周杰伦");
    EXPECT_EQ(r.title.value, "我的地盘");
    EXPECT_EQ(r.album.value, "七里香");
}

TEST(MetadataInference, Normalizer_Key) {
    EXPECT_EQ(MetadataNormalizer::normalizeKey(" The Beatles "), "the beatles");
    EXPECT_EQ(MetadataNormalizer::normalizeKey("THE BEATLES"), "the beatles");
}

TEST(MetadataInference, FullWidth_To_Half) {
    EXPECT_EQ(MetadataNormalizer::normalizeFullWidth("－"), "-");
    EXPECT_EQ(MetadataNormalizer::normalizeFullWidth("　x"), " x");
}

#endif // UNIT_TEST