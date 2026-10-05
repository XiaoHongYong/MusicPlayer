#include "SkinOs.h"
#include "../Utils/SimpleXML.h"

cstr_t getSkinOsName() {
#ifdef _WIN32
    return "win";
#elif defined(_MAC_OS)
    return "mac";
#else
    return "linux";
#endif
}

static bool osTokenMatches(cstr_t token, cstr_t cur) {
    if (strcasecmp(token, cur) == 0) {
        return true;
    }
    if (strcmp(cur, "win") == 0 && strcasecmp(token, "windows") == 0) {
        return true;
    }
    if (strcmp(cur, "mac") == 0 &&
        (strcasecmp(token, "macos") == 0 || strcasecmp(token, "osx") == 0)) {
        return true;
    }
    return false;
}

bool isSkinOsMatch(cstr_t osList) {
    if (isEmptyString(osList)) {
        return true;
    }

    VecStrings parts;
    strSplit(osList, ',', parts);
    trimStr(parts);

    cstr_t cur = getSkinOsName();
    for (const string &part : parts) {
        if (!part.empty() && osTokenMatches(part.c_str(), cur)) {
            return true;
        }
    }
    return false;
}

bool isSkinXmlNodeForCurrentOs(SXNode *node) {
    if (node == nullptr) {
        return true;
    }
    return isSkinOsMatch(node->getProperty("os"));
}

static bool isSkinOsSuffix(cstr_t token) {
    return osTokenMatches(token, "win") || osTokenMatches(token, "mac") ||
        osTokenMatches(token, "linux") || strcasecmp(token, "linux") == 0;
}

SkinStyleOsKind parseSkinStyleName(cstr_t rawName, string &baseName) {
    baseName = rawName ? rawName : "";
    if (baseName.empty()) {
        return SkinStyleOsKind::Generic;
    }

    auto dot = baseName.rfind('.');
    if (dot == string::npos || dot == 0 || dot + 1 >= baseName.size()) {
        return SkinStyleOsKind::Generic;
    }

    string suffix = baseName.substr(dot + 1);
    if (!isSkinOsSuffix(suffix.c_str())) {
        return SkinStyleOsKind::Generic;
    }

    baseName.resize(dot);
    if (osTokenMatches(suffix.c_str(), getSkinOsName())) {
        return SkinStyleOsKind::Current;
    }
    return SkinStyleOsKind::Other;
}

#if UNIT_TEST

#include "utils/unittest.h"

TEST(SkinOs, isSkinOsMatch) {
    ASSERT_TRUE(isSkinOsMatch(nullptr));
    ASSERT_TRUE(isSkinOsMatch(""));
    ASSERT_TRUE(isSkinOsMatch(getSkinOsName()));

    string all = "win, mac, linux";
    ASSERT_TRUE(isSkinOsMatch(all.c_str()));

    ASSERT_FALSE(isSkinOsMatch("nope"));

#ifdef _MAC_OS
    ASSERT_TRUE(isSkinOsMatch("macos"));
    ASSERT_TRUE(isSkinOsMatch("osx"));
    ASSERT_FALSE(isSkinOsMatch("win"));
    ASSERT_FALSE(isSkinOsMatch("linux"));
#elif defined(_WIN32)
    ASSERT_TRUE(isSkinOsMatch("windows"));
    ASSERT_FALSE(isSkinOsMatch("mac"));
#else
    ASSERT_FALSE(isSkinOsMatch("mac"));
    ASSERT_FALSE(isSkinOsMatch("win"));
#endif
}

TEST(SkinOs, parseSkinStyleName) {
    string base;
    ASSERT_EQ(parseSkinStyleName("Caption", base), SkinStyleOsKind::Generic);
    ASSERT_STREQ(base.c_str(), "Caption");

    ASSERT_EQ(parseSkinStyleName("NormalListCtrl", base), SkinStyleOsKind::Generic);

    auto macKind = parseSkinStyleName("Caption.mac", base);
    ASSERT_STREQ(base.c_str(), "Caption");
#ifdef _MAC_OS
    ASSERT_EQ(macKind, SkinStyleOsKind::Current);
#else
    ASSERT_EQ(macKind, SkinStyleOsKind::Other);
#endif

    auto winKind = parseSkinStyleName("DialogCaption.win", base);
    ASSERT_STREQ(base.c_str(), "DialogCaption");
#ifdef _WIN32
    ASSERT_EQ(winKind, SkinStyleOsKind::Current);
#else
    ASSERT_EQ(winKind, SkinStyleOsKind::Other);
#endif

    auto linuxKind = parseSkinStyleName("MenuCaption.linux", base);
    ASSERT_STREQ(base.c_str(), "MenuCaption");
    if (strcmp(getSkinOsName(), "linux") == 0) {
        ASSERT_EQ(linuxKind, SkinStyleOsKind::Current);
    } else {
        ASSERT_EQ(linuxKind, SkinStyleOsKind::Other);
    }
}

#endif
