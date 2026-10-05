#include "M4aFileTags.hpp"
#include "ID3/ID3v1.h"
#include "ID3/ID3v2.h"
#include "ID3/ID3Helper.h"
#include "../ImageLib/RawImageData.h"
#include "MediaTags.h"
#include "LrcParser.h"


namespace MediaTags {

//
// References:
// http://atomicparsley.sourceforge.net/mpeg-4files.html
//

#define SZ_TYPE_MOOV        "moov"
#define SZ_TYPE_TRACK       "trak"
#define SZ_TYPE_UDTA        "udta"
#define SZ_TYPE_META        "meta"
#define SZ_TYPE_ILST        "ilst"
#define SZ_TYPE_LYRICS      "\xA9lyr"
#define SZ_TYPE_PICTRUE     "covr"
#define SZ_TYPE_STSD        "stsd"

#define SZ_TYPE_MP4A        "mp4a"

enum AtomDataType {
    ADT_IMPLICIT  = 0,  // for use with tags for which no type needs to be indicated because only one type is allowed
    ADT_UTF8      = 1,  // without any count or null terminator
    ADT_UTF16     = 2,  // also known as UTF-16BE
    ADT_SJIS      = 3,  // deprecated unless it is needed for special Japanese characters
    ADT_HTML      = 6,  // the HTML file header specifies which HTML version
    ADT_XML       = 7,  // the XML header must identify the DTD or schemas
    ADT_UUID      = 8,  // also known as GUID; stored as 16 bytes in binary (valid as an ID)
    ADT_ISRC      = 9,  // stored as UTF-8 text (valid as an ID)
    ADT_MI3P      = 10, // stored as UTF-8 text (valid as an ID)
    ADT_GIF       = 12, // (deprecated) a GIF image
    ADT_JPEG      = 13, // a JPEG image
    ADT_PNG       = 14, // a PNG image
    ADT_URL       = 15, // absolute, in UTF-8 characters
    ADT_DURATION  = 16, // in milliseconds, 32-bit integer
    ADT_DATETIME  = 17, // in UTC, counting seconds since midnight, January 1, 1904; 32 or 64-bits
    ADT_GENRED    = 18, // a list of enumerated values
    ADT_INTEGER   = 21, // a signed big-endian integer with length one of { 1,2,3,4,8 } bytes
    ADT_RIAAPA    = 24, // RIAA parental advisory; { -1=no, 1=yes, 0=unspecified }, 8-bit integer
    ADT_UPC       = 25, // Universal Product Code, in text UTF-8 format (valid as an ID)
    ADT_BMP       = 27, // Windows bitmap image
    ADT_UNDEFINED = 255 // undefined
};

class M4aBox {
public:
    M4aBox(cstr_t szType, size_t offset, size_t size);
    virtual ~M4aBox();

    typedef vector<M4aBox*> VecBoxes;
    enum {
        TYPE_SIZE                   = 4,
        HEADER_SIZE                 = 8,
    };

    void addChild(M4aBox *box);
    M4aBox *addNewBox(cstr_t type);

    bool isType(cstr_t szType);

    M4aBox *findBox(cstr_t szBox);
    M4aBox *nextBox(M4aBox *brother);

    bool deleteChild(cstr_t szType);
    bool deleteChild(M4aBox *child);

    void clearChildren();

    void toBuf(string &buf);

    void calculateNewSize();

    size_t getDataSize() { return m_nSize - HEADER_SIZE; }

    int parseDataBox(uint32_t &typeOut, string &dataOut);
    void setDataBox(uint32_t type, const StringView &data);
    int parseDataBox(VecInts &vTypesOut, VecStrings &vDataOut);
    void setDataBox(const VecInts &types, const VecStrings &vData);

    int getDataUtf8String(string &out);
    void setDataUtf8String(const string &data);

    int getDataPictures(VecStrings &picsOut);
    void setDataPictures(const VecStrings &pics);

    vector<uint16_t> getDataUint16Arr();
    void setDataUint16Arr(const uint16_t *arr, size_t len);

public:
    size_t                      m_nOffset;          // The offset of the box
    size_t                      m_nSize;            // The size of the whole box.
    char                        m_szType[TYPE_SIZE + 1];
    string                      m_data;

    size_t                      m_nSizeNew;

    VecBoxes                    m_vChildren;

};

class M4aTag {
public:
    M4aTag();
    virtual ~M4aTag();

    int open(cstr_t szFile, bool bModify);
    void close();
    int save();

    int getTags(BasicMediaTags &tags);
    int setTags(const BasicMediaTags &tags);

    const ExtendedMediaInfo &getExtMediaInfo() const { return _extInfo; }

    int listLyrics(VecStrings &vLyrNames);
    int getLyrics(string &strLyrics);

    bool hasLyrics();
    int setLyrics(const string &lyrics);
    int removeLyrics();

    int setPictures(const VecStrings &pictures);
    VecStrings getPictures();

    M4aBox &getRoot() { return m_root; }

    M4aBox *getBox(cstr_t szPath);

protected:
    int parse(M4aBox *parent, int nExtraHeaderOffset = M4aBox::HEADER_SIZE);
    void parseSTSD(M4aBox *box);
    void parseMDHD(M4aBox *box);
    void parseMVHD(M4aBox *box);

    int adjustStco(int nAdjustedOffset);

    int read(M4aBox *parent);
    int write(string &buf, size_t offset);

    FILE                        *m_fp = nullptr;
    M4aBox                      m_root;             // The virtual root box

    ExtendedMediaInfo           _extInfo;

};


M4aBox *getBox(M4aBox *parent, cstr_t szPath) {
    VecStrings vPath;
    strSplit(szPath, '.', vPath);

    for (uint32_t i = 0; i < vPath.size(); i++) {
        parent = parent->findBox(vPath[i].c_str());
        if (!parent) {
            return nullptr;
        }
    }

    return parent;
}

bool canBoxHasChild(char szType[]) {
    static cstr_t vList[] = {
        // We only need to parse these parent boxes
        "moov", "trak", "mdia", "minf", "stbl", "udta", "meta", "ilst",
        // "moov", "udta", "mdia", "meta", "ilst", "stbl", "minf", "moof", "traf", "trak", "stsd",
    };

    for (int i = 0; i < CountOf(vList); i++) {
        if (strncmp(szType, vList[i], M4aBox::TYPE_SIZE) == 0) {
            return true;
        }
    }

    return false;
}

// https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/Metadata/Metadata.html

M4aBox::M4aBox(cstr_t szType, size_t offset, size_t size) {
    m_nOffset = offset;
    m_nSize = size;
    strncpy_safe(m_szType, CountOf(m_szType), szType, M4aBox::TYPE_SIZE);

    assert(sizeof(m_szType) >= TYPE_SIZE);
    m_nSizeNew = 0;
}

M4aBox::~M4aBox() {
    clearChildren();
}

void M4aBox::addChild(M4aBox *box) {
    m_vChildren.push_back(box);
}

M4aBox *M4aBox::addNewBox(cstr_t type) {
    auto child = new M4aBox(type, 0, 0);
    addChild(child);
    return child;
}

bool M4aBox::isType(cstr_t szType) {
    return strncmp(m_szType, szType, TYPE_SIZE) == 0;
}

void M4aBox::clearChildren() {
    for (uint32_t i = 0; i < m_vChildren.size(); i++) {
        delete m_vChildren[i];
    }
    m_vChildren.clear();
}

M4aBox *M4aBox::findBox(cstr_t szBox) {
    for (uint32_t i = 0; i < m_vChildren.size(); i++) {
        M4aBox *p = m_vChildren[i];
        if (p->isType(szBox)) {
            return p;
        }
    }

    return nullptr;
}

M4aBox *M4aBox::nextBox(M4aBox *brother) {
    for (uint32_t i = 0; i < m_vChildren.size(); i++) {
        M4aBox *p = m_vChildren[i];
        if (p == brother) {
            if (i + 1 < m_vChildren.size()) {
                return m_vChildren[i + 1];
            }
        }
    }

    return nullptr;
}

bool M4aBox::deleteChild(cstr_t szType) {
    for (uint32_t i = 0; i < m_vChildren.size(); i++) {
        M4aBox *p = m_vChildren[i];
        if (p->isType(szType)) {
            delete p;
            m_vChildren.erase(m_vChildren.begin() + i);
            return true;
        }
    }

    return false;
}

bool M4aBox::deleteChild(M4aBox *child) {
    for (uint32_t i = 0; i < m_vChildren.size(); i++) {
        M4aBox *p = m_vChildren[i];
        if (p == child) {
            delete p;
            m_vChildren.erase(m_vChildren.begin() + i);
            return true;
        }
    }

    return false;
}

void M4aBox::toBuf(string &buf) {
    // size
    if (m_nSizeNew == 0) {
        calculateNewSize();
    }

    uint8_t size[4];
    uint32ToBE((uint32_t)m_nSizeNew, size);
    buf.append((char *)size, M4aBox::TYPE_SIZE);

    // Type
    buf.append(m_szType, M4aBox::TYPE_SIZE);

    if (m_vChildren.size() == 0) {
        buf.append(m_data);
        return;
    }

    // Data
    if (m_data.size() > 0) {
        assert(isType(SZ_TYPE_META));
        buf.append(m_data);
    }

    // Children
    for (uint32_t i = 0; i < m_vChildren.size(); i++) {
        M4aBox *p = m_vChildren[i];
        p->toBuf(buf);
    }
}

void M4aBox::calculateNewSize() {
    m_nSizeNew = HEADER_SIZE + m_data.size();

    for (uint32_t i = 0; i < m_vChildren.size(); i++) {
        M4aBox *p = m_vChildren[i];
        p->calculateNewSize();
        m_nSizeNew += p->m_nSizeNew;
    }
}

int parseDataBoxStream(BinaryInputStream &is, uint32_t &typeOut, string &dataOut) {
    try {
        int length = is.readUInt32BE();
        auto name = is.readString(4);
        if (!name.equal("data")) {
            return ERR_BAD_FILE_FORMAT;
        }

        typeOut = is.readUInt32BE();
        if (typeOut >= ADT_UNDEFINED) {
            return ERR_BAD_FILE_FORMAT;
        }

        is.forward(4); // ignore locale

        auto s = is.readString(length - M4aBox::HEADER_SIZE - 8);
        dataOut.assign(s.data, s.len);
        return ERR_OK;
    } catch (std::exception &e) {
        return ERR_BAD_FILE_FORMAT;
    }
}

void appendDataBox(uint32_t type, const StringView &data, string &dataOut) {
    uint32_t length = M4aBox::HEADER_SIZE + 8 + data.len;

    BinaryOutputStream os;

    os.writeUInt32BE(length); // data box length.
    os.write("data");

    os.writeUInt32BE(type); // Data type
    os.writeUInt32BE(0); // locale

    os.write(data.data, data.len);

    auto s = os.toStringView();

    dataOut.append(s.data, s.len);
}

int M4aBox::parseDataBox(uint32_t &typeOut, string &dataOut) {
    BinaryInputStream is(m_data);

    return parseDataBoxStream(is, typeOut, dataOut);
}

void M4aBox::setDataBox(uint32_t type, const StringView &data) {
    m_data.clear();
    appendDataBox(type, data, m_data);
}

int M4aBox::parseDataBox(VecInts &vTypesOut, VecStrings &vDataOut) {
    try {
        BinaryInputStream is(m_data);

        while (is.isRemaining()) {
            uint32_t type;
            string data;
            int ret = parseDataBoxStream(is, type, data);
            if (ret != ERR_OK) {
                return ret;
            }

            vTypesOut.push_back(type);
            vDataOut.push_back(data);
        }

        return ERR_OK;
    } catch (std::exception &e) {
        return ERR_BAD_FILE_FORMAT;
    }
}

void M4aBox::setDataBox(const VecInts &types, const VecStrings &vData) {
    assert(types.size() == vData.size());
    m_data.clear();

    for (int i = 0; i < types.size() && i < vData.size(); i++) {
        appendDataBox(types[i], vData[i], m_data);
    }
}

int M4aBox::getDataUtf8String(string &out) {
   uint32_t type;
   int ret = parseDataBox(type, out);
   if (ret != ERR_OK) {
       return ret;
   }

   if (type != ADT_UTF8) {
       out.clear();
       return ERR_BAD_FILE_FORMAT;
   }

   return ERR_OK;
}

void M4aBox::setDataUtf8String(const string &data) {
    setDataBox(ADT_UTF8, data);
}

int M4aBox::getDataPictures(VecStrings &picsOut) {
    VecInts types;
    VecStrings vData;

    int ret = parseDataBox(types, vData);
    if (ret == ERR_OK) {
        for (int i = 0; i < types.size(); i++) {
            auto type = types[i];
            if (type == ADT_PNG || type == ADT_JPEG || type == ADT_GIF || type == ADT_BMP) {
                picsOut.push_back(vData[i]);
            }
        }
    }

    return ret;
}

AtomDataType getImageDataType(const StringView &data) {
    StringView ext = guessPictureDataExt(data);
    if (ext.equal(".png")) {
        return ADT_PNG;
    } else if (ext.equal(".gif")) {
        return ADT_GIF;
    } else if (ext.equal(".bmp")) {
        return ADT_BMP;
    } else {
        return ADT_JPEG;
    }
}

void M4aBox::setDataPictures(const VecStrings &pics) {
    VecInts types;
    VecStrings vData;

    for (auto &pic : pics) {
        types.push_back(getImageDataType(pic));
        vData.push_back(pic);
    }

    setDataBox(types, vData);
}

vector<uint16_t> M4aBox::getDataUint16Arr() {
    uint32_t type;
    string out;
    int ret = parseDataBox(type, out);
    if (ret != ERR_OK || type != ADT_IMPLICIT) {
        return {};
    }

    // 用于测试
    // string s;
    // for (; p < end; p += 4) {
    //     s += itos(uint32FromBE(p)) + ",";
    // }
    // printf("N32: %s\n", s.c_str());
    //
    // s.clear(); p = (uint8_t *)m_data.c_str() + 8;
    // for (; p < end; p += 2) {
    //     s += itos(uint16FromBE(p)) + ",";
    // }
    // printf("N16: %s\n", s.c_str());
    //
    // s.clear(); p = (uint8_t *)m_data.c_str() + 8;
    // for (; p < end; p ++) {
    //     s += itos(*p) + ",";
    // }
    // printf("N8: %s\n", s.c_str());

    vector<uint16_t> a;

    uint8_t *p = (uint8_t *)out.c_str(), *end = p + out.size();
    for (; p < end; p += 2) {
        a.push_back(uint16FromBE(p));
    }

    return a;
}

void M4aBox::setDataUint16Arr(const uint16_t *arr, size_t len) {
    BinaryOutputStream os;
    for (size_t i = 0; i < len; i++) {
        os.writeUInt16BE(arr[i]);
    }

    setDataBox(ADT_IMPLICIT, os.toStringView());
}

void printfBoxTree(M4aBox *root, int depth = 0) {
    for (auto &child : root->m_vChildren) {
        printf("%*s, %d\n", depth * 2 + 4, child->m_szType, (int)child->m_nSize);
        printfBoxTree(child, depth + 1);
    }
}

//////////////////////////////////////////////////////////////////////////

M4aTag::M4aTag() : m_root("", 0, 0) {
}

M4aTag::~M4aTag() {
    close();
}

int M4aTag::open(cstr_t szFile, bool bModify) {
    m_fp = fopenUtf8(szFile, bModify ? "r+b" : "rb");

    if (!m_fp) {
        setCustomErrorDesc(stringPrintf("%s: %s", getLastSysErrorDesc().c_str(), szFile).c_str());
        return ERR_CUSTOM_ERROR;
    }

    fseek(m_fp, 0, SEEK_END);
    size_t nFileSize = ftell(m_fp);
    fseek(m_fp, 0, SEEK_SET);

    m_root.m_nOffset = 0;
    m_root.m_nSize = nFileSize;
    int nRet = parse(&m_root, 0);
    if (nRet != ERR_OK) {
        return nRet;
    }

    // printfBoxTree(&m_root);

    // read moov box immediately
    M4aBox *boxMoov = m_root.findBox(SZ_TYPE_MOOV);
    if (boxMoov == nullptr) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    auto box = getBox("moov.trak.mdia.minf.stbl.stsd");
    if (box) {
        // Sample description box
        parseSTSD(box);
    }

    box = getBox("moov.trak.mdia.mdhd");
    if (box) {
        // track header
        parseMDHD(box);
    }

    if (_extInfo.mediaLength <= 0) {
        box = getBox("moov.mvhd");
        if (box) {
            // movie header
            parseMVHD(box);
        }
    }

    return read(boxMoov);
}


static uint64_t readAtomHeader(BinaryInputStream &stream, uint8_t *atomType) {
    uint64_t size = stream.readUInt32BE();

    memcpy(atomType, stream.currentPtr(), 4);
    stream.forward(4);

    if (size == 1) {
        // 64bit atom size
        size = stream.readUInt64BE();
    }

    return size;
}

static uint32_t readDescrLength(BinaryInputStream &stream) {
    uint8_t b;
    uint8_t numBytes = 0;
    uint32_t length = 0;

    do
    {
        b = stream.readUInt8();
        numBytes++;
        length = (length << 7) | (b & 0x7F);
    } while ((b & 0x80) && numBytes < 4);

    return length;
}


static void readEsds(BinaryInputStream &stream, int &avgBitrate) {
    stream.readUInt32BE(); // version(1) + flags(3)

    // ES_DescrTag
    auto tag = stream.readUInt8();
    if (tag == 0x03) {
        // length
        if (readDescrLength(stream) < 5 + 15) {
            return;
        }
        // skip 3 bytes
        stream.forward(3);
    } else {
        /* skip 2 bytes */
        stream.forward(2);
    }

    // DecoderConfigDescrTab
    if (stream.readUInt8() != 0x04) {
        return;
    }

    if (readDescrLength(stream) < 13) {
        return;
    }

    stream.readUInt8(); // audioType
    stream.readUInt32BE(); // 0x15000414 ????
    stream.readUInt32BE(); // maxBitrate
    avgBitrate = (stream.readUInt32BE() + 999) / 1000;

    // DecSpecificInfoTag
    if (stream.readUInt8() != 0x05) {
        return;
    }
}

void M4aTag::parseSTSD(M4aBox *box) {
    unique_ptr<uint8_t> buf(new uint8_t[box->m_nSize]);

    fseek(m_fp, box->m_nOffset + M4aBox::HEADER_SIZE, SEEK_SET);
    auto read = fread(buf.get(), 1, box->m_nSize, m_fp);
    if (read != box->m_nSize) {
        return;
    }

    BinaryInputStream stream(buf.get(), box->m_nSize);

    try {
        stream.readUInt32BE(); // version(1) + flags(3)

        auto count = stream.readUInt32BE();
        for (uint32_t i = 0; i < count; i++) {
            uint8_t type[4];

            auto size = readAtomHeader(stream, type);
            auto offset = stream.offset();

            if (memcmp(type, SZ_TYPE_MP4A, 4) == 0) {
                stream.forward(6); // reserved
                stream.readUInt16BE(); // data_reference_index
                stream.readUInt32BE(); // reserved
                stream.readUInt32BE(); // reserved

                _extInfo.channels = stream.readUInt16BE();
                _extInfo.bitsPerSample = stream.readUInt16BE();

                stream.readUInt16BE(); // Unkown
                stream.readUInt16BE(); // Unkown

                _extInfo.sampleRate = stream.readUInt16BE();

                stream.readUInt16BE(); // Unkown

                readAtomHeader(stream, type);
                if (memcmp(type, "esds", 4) == 0) {
                    readEsds(stream, _extInfo.bitRate);
                }
            }

            stream.setOffset((uint32_t)(offset + size));
        }
    } catch (exception &e) {
        //
        ERR_LOG1("Got exception: %s", e.what());
    }
}

void M4aTag::parseMDHD(M4aBox *box) {
    unique_ptr<uint8_t> buf(new uint8_t[box->m_nSize]);

    fseek(m_fp, box->m_nOffset + M4aBox::HEADER_SIZE, SEEK_SET);
    auto read = fread(buf.get(), 1, box->m_nSize, m_fp);
    if (read != box->m_nSize) {
        return;
    }

    BinaryInputStream stream(buf.get(), box->m_nSize);

    try {
        uint32_t version = stream.readUInt32BE();
        if (version == 1) {
            stream.readUInt64BE(); // creation-time
            stream.readUInt64BE(); // modification-time
            auto scale = stream.readUInt32BE(); // timescale
            _extInfo.mediaLength = uint32_t(stream.readUInt64BE() * 1000.0 / scale + 0.5);
        } else if (version == 0) {
            stream.readUInt32BE(); // creation-time
            stream.readUInt32BE(); // modification-time
            auto scale = stream.readUInt32BE(); // timescale
            _extInfo.mediaLength = uint32_t(stream.readUInt32BE() * 1000.0 / scale + 0.5);
        } else {
            assert(0 && "NOT supported version.");
        }
    } catch (exception &e) {
        ERR_LOG1("Got exception: %s", e.what());
    }
}

void M4aTag::parseMVHD(M4aBox *box) {
    unique_ptr<uint8_t> buf(new uint8_t[box->m_nSize]);

    fseek(m_fp, box->m_nOffset + M4aBox::HEADER_SIZE, SEEK_SET);
    auto read = fread(buf.get(), 1, box->m_nSize, m_fp);
    if (read != box->m_nSize) {
        return;
    }

    BinaryInputStream stream(buf.get(), box->m_nSize);

    try {
        stream.readUInt32(); // version + flags
        stream.readUInt32BE(); // creation-time
        stream.readUInt32BE(); // modification-time
        auto scale = stream.readUInt32BE(); // timescale
        _extInfo.mediaLength = (uint32_t)(stream.readUInt32BE() * 1000.0 / scale);
    } catch (exception &e) {
        ERR_LOG1("Got exception: %s", e.what());
    }
}

void M4aTag::close() {
    if (m_fp) {
        fclose(m_fp);
        m_fp = nullptr;
    }
}

int M4aTag::save() {
    M4aBox *boxMoov = m_root.findBox(SZ_TYPE_MOOV);
    if (boxMoov == nullptr) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    if (getBox("moov.trak.mdia.minf.stbl.co64") != nullptr) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    boxMoov->calculateNewSize();

    long nAdjustedOffset = 0;
    size_t nSizeNew = boxMoov->m_nSizeNew;
    if (boxMoov->m_nSize != nSizeNew) {
        // Need to resize boxes
        M4aBox *free = m_root.nextBox(boxMoov);
        if (free == nullptr) {
            // resize
            int nRet = fileMoveEndData(m_fp, boxMoov->m_nOffset + boxMoov->m_nSize, (uint32_t)(boxMoov->m_nOffset + nSizeNew));
            if (nRet != ERR_OK) {
                return nRet;
            }
            nAdjustedOffset = nSizeNew - boxMoov->m_nSize;
        } else {
            if (boxMoov->m_nSize + free->m_nSize + M4aBox::HEADER_SIZE < nSizeNew) {
                // resize
                int nRet = fileMoveEndData(m_fp, boxMoov->m_nOffset + boxMoov->m_nSize, boxMoov->m_nOffset + nSizeNew);
                if (nRet != ERR_OK) {
                    return nRet;
                }
                nAdjustedOffset = nSizeNew - boxMoov->m_nSize;
            } else {
                // resize free box
                free->m_nOffset = boxMoov->m_nOffset + nSizeNew;
                free->m_nSize -= nSizeNew - boxMoov->m_nSize;
                free->m_data.resize(free->getDataSize(), 0);

                string bufFree;
                free->toBuf(bufFree);
                int nRet = write(bufFree, free->m_nOffset);
                if (nRet != ERR_OK) {
                    return nRet;
                }
            }
        }
    }

    if (nAdjustedOffset != 0) {
        adjustStco((int)nAdjustedOffset);
    }

    string buf;
    boxMoov->toBuf(buf);

    int nRet = write(buf, boxMoov->m_nOffset);
    if (nRet != ERR_OK) {
        return nRet;
    }

    close();

    return ERR_OK;
}

int M4aTag::parse(M4aBox *parent, int nExtraHeaderOffset) {

    size_t totalSize = nExtraHeaderOffset;
    size_t offset = parent->m_nOffset + nExtraHeaderOffset;

    while (true) {
        fseek(m_fp, offset, SEEK_SET);

        // read size
        uint8_t buf[M4aBox::TYPE_SIZE];
        auto read = fread(buf, 1, M4aBox::TYPE_SIZE, m_fp);
        if (read != M4aBox::TYPE_SIZE) {
            return ERR_BAD_FILE_FORMAT;
        }

        size_t size = uint32FromBE(buf);
        if (size < M4aBox::HEADER_SIZE) {
            return ERR_BAD_FILE_FORMAT;
        }

        // read type
        char type[M4aBox::TYPE_SIZE];
        read = fread(type, 1, M4aBox::TYPE_SIZE, m_fp);
        if (read != M4aBox::TYPE_SIZE) {
            return ERR_BAD_FILE_FORMAT;
        }

        totalSize += size;
        if (totalSize > parent->m_nSize) {
            return ERR_BAD_FILE_FORMAT;
        }

        M4aBox *pChild = new M4aBox(type, offset, size);

        offset += size;
        parent->addChild(pChild);

        // printf("Box: %.4s, %.4s, offset: %d, size: %d\n", parent->m_szType, type, offset, size);

        if (totalSize == parent->m_nSize) {
            break;
        }
    }

    for (uint32_t i = 0; i < parent->m_vChildren.size(); i++) {
        M4aBox *pChild = parent->m_vChildren[i];

        if (canBoxHasChild(pChild->m_szType)) {
            // 只解析关心的 box
            int nExtraHeaderOffset = M4aBox::HEADER_SIZE;
            if (pChild->isType(SZ_TYPE_META)) {
                nExtraHeaderOffset += 4;
            }
            int nRet = parse(pChild, nExtraHeaderOffset);
            if (nRet != ERR_OK) {
                pChild->clearChildren();
            }
        }
    }

    return ERR_OK;
}

void getBoxText(M4aBox *box, string &value) {
    if (value.empty()) {
        box->getDataUtf8String(value);
    }
}

void setBoxText(M4aBox *box, cstr_t value) {
    assert(box->m_vChildren.empty());
    box->m_vChildren.clear();

    box->setDataUtf8String(value);
}

void getBoxTrackNumber(M4aBox *box, string &value) {
    if (value.empty()) {
        auto arr = box->getDataUint16Arr();
        if (arr.size() == 4) {
            value = itos(arr[1]) + "," + itos(arr[2]);
        }
    }
}

void setBoxTrackNumber(M4aBox *box, cstr_t value) {
    string a, b;
    int n1 = 0, n2 = 0;
    if (strSplit(value, ',', a, b)) {
        n1 = atoi(a.c_str());
        n2 = atoi(b.c_str());
    } else {
        n1 = atoi(value);
    }

    auto arr = box->getDataUint16Arr();
    if (arr.size() < 4) {
        arr.resize(4);
    }

    arr[1] = n1;
    arr[2] = n2;

    box->setDataUint16Arr(arr.data(), arr.size());
}

void getBoxGenre(M4aBox *parent, string &value) {
    if (value.empty()) {
        uint32_t type;
        string data;
        int ret = parent->parseDataBox(type, data);
        if (ret == ERR_OK) {
            if (type == ADT_IMPLICIT) {
                auto arr = parent->getDataUint16Arr();
                if (arr.size() == 1) {
                    // ID3v1 是从 0 开始，m4a 从 1 开始
                    value = CID3v1::getGenreDescription(arr[0] - 1);
                }
            } else if (type == ADT_UTF8) {
                value = data;
            }
        }
    }
}

void setBoxGenre(M4aBox *parent, cstr_t value) {
    uint32_t type;
    string data;
    int ret = parent->parseDataBox(type, data);
    if (ret == ERR_OK) {
        if (type == ADT_IMPLICIT) {
            auto arr = parent->getDataUint16Arr();
            if (arr.size() == 1) {
                int index = CID3v1::getGenreIndex(value);
                if (index == -1) {
                    return;
                }
                // ID3v1 是从 0 开始，m4a 从 1 开始
                arr[0] = index + 1;

                parent->setDataUint16Arr(arr.data(), arr.size());
            }
        } else if (type == ADT_UTF8) {
            parent->setDataUtf8String(value);
        }
    }
}

int M4aTag::getTags(BasicMediaTags &tags) {

    M4aBox *parent = getBox("moov.udta.meta.ilst");
    if (!parent) {
        return ERR_NOT_FOUND;
    }

    for (auto child : parent->m_vChildren) {
        // printf("%s\n", child->m_szType);

        if (child->isType("\xA9""art")) { getBoxText(child, tags.artist); }
        else if (child->isType("\xA9""ART")) { getBoxText(child, tags.artist); }
        else if (child->isType("aART")) { getBoxText(child, tags.artist); }
        else if (child->isType("\xA9""nam")) { getBoxText(child, tags.title); }
        else if (child->isType("\xA9""NAM")) { getBoxText(child, tags.title); }
        else if (child->isType("\xA9""alb")) { getBoxText(child, tags.album); }
        else if (child->isType("\xA9""ALB")) { getBoxText(child, tags.album); }
        else if (child->isType("\xA9""day")) { getBoxText(child, tags.year); }
        else if (child->isType("\xA9""gen")) { getBoxText(child, tags.genre); }
        else if (child->isType("gnre")) { getBoxGenre(child, tags.genre); }
        else if (child->isType("trkn")) { getBoxTrackNumber(child, tags.trackNo); }
    }

    return ERR_OK;
}

void setChildBoxText(M4aBox *parent, cstr_t text, cstr_t *types) {
    for (auto child : parent->m_vChildren) {
        for (auto p = types; *p != nullptr; p++) {
            if (child->isType(*p)) {
                setBoxText(child, text);
                return;
            }
        }
    }

    auto child = parent->addNewBox(types[0]);
    setBoxText(child, text);
}

int M4aTag::setTags(const BasicMediaTags &tags) {
    M4aBox *parent = getBox("moov.udta.meta.ilst");
    if (!parent) {
        return ERR_NOT_FOUND;
    }

    static cstr_t TYPES_ARTIST[] = { "\xA9""art", "\xA9""ART", "aART", nullptr };
    static cstr_t TYPES_TITLE[] = { "\xA9""nam", "\xA9""NAM", nullptr };
    static cstr_t TYPES_ALBUM[] = { "\xA9""alb", "\xA9""ALB", nullptr };
    static cstr_t TYPES_YEAR[] = { "\xA9""day", nullptr };
    static cstr_t TYPES_GENRE[] = { "\xA9""gen", nullptr };

    setChildBoxText(parent, tags.artist.c_str(), TYPES_ARTIST);
    setChildBoxText(parent, tags.title.c_str(), TYPES_TITLE);
    setChildBoxText(parent, tags.album.c_str(), TYPES_ALBUM);
    setChildBoxText(parent, tags.year.c_str(), TYPES_YEAR);
    setChildBoxText(parent, tags.genre.c_str(), TYPES_GENRE);

    auto box = parent->findBox("gnre");
    if (box) {
        setBoxGenre(box, tags.genre.c_str());
    }

    box = parent->findBox("trkn");
    if (box) {
        setBoxTrackNumber(box, tags.trackNo.c_str());
    } else {
        box = parent->addNewBox("trkn");
        setBoxTrackNumber(box, tags.trackNo.c_str());
    }

    return ERR_OK;
}

/**
stco = sample table chunk offset

QT Atom Preamble
1 uint8_t    version
3 bytes   flags
4 bytes   total entries in offset table (n)
4 bytes   chunk offset 0
4 bytes   chunk offset 1
..
..
4 bytes   chunk offset n-1
*/
int M4aTag::adjustStco(int nAdjustedOffset) {
    M4aBox *pBoxStco = getBox("moov.trak.mdia.minf.stbl.stco");
    if (pBoxStco != nullptr) {
        const int offsetTrunk = 8;
        int count = (int)(pBoxStco->m_data.size() - offsetTrunk) / 4;
        int countShould = uint32FromBE((uint8_t *)(pBoxStco->m_data.c_str() + 4));
        assert(count == countShould);
        if (count > countShould) {
            count = countShould;
        }

        int end = offsetTrunk + count * 4;
        for (int i = offsetTrunk; i < end; i += 4) {
            uint32_t n = uint32FromBE((uint8_t *)(pBoxStco->m_data.c_str() + i));
            uint32ToBE(n + nAdjustedOffset, (uint8_t *)(pBoxStco->m_data.c_str() + i));
        }
    }

    return ERR_OK;
}

void read(FILE *fp, string &buf, size_t offset, size_t size) {
    fseek(fp, offset, SEEK_SET);
    buf.resize(size);
    auto n = fread((char *)buf.c_str(), 1, size, fp);
    if (n != size) {
        buf.resize(0);
    }
}

#ifdef _DEBUG
// This is used to verify the data read can composed are OK.
void verifyData(FILE *fp, M4aBox *box) {
    string buf;
    read(fp, buf, box->m_nOffset, box->m_nSize);
    assert(buf.size() == box->m_nSize);

    string buf2;
    box->toBuf(buf2);
    assert(buf.size() == buf2.size());
    assert(memcmp(buf.c_str(), buf2.c_str(), buf.size()) == 0);
}
#endif // #ifdef _DEBUG

int M4aTag::read(M4aBox *parent) {
    // Assure the box is correct, we don't verify again.
    if (parent->m_vChildren.size() == 0) {
        parent->m_data.resize(parent->getDataSize());
        fseek(m_fp, parent->m_nOffset + M4aBox::HEADER_SIZE, SEEK_SET);
        auto n = fread((char *)parent->m_data.c_str(), 1, parent->m_data.size(), m_fp);
        if (n != parent->m_data.size()) {
            return ERR_READ_FILE;
        }

        // verifyData(m_fp, parent);

        return ERR_OK;
    }

    if (parent->isType(SZ_TYPE_META)) {
        parent->m_data.resize(4);
        fseek(m_fp, parent->m_nOffset + M4aBox::HEADER_SIZE, SEEK_SET);
        auto n = fread((char *)parent->m_data.c_str(), 1, 4, m_fp);
        if (n != 4) {
            return ERR_READ_FILE;
        }
    }

    for (uint32_t i = 0; i < parent->m_vChildren.size(); i++) {
        M4aBox *pChild = parent->m_vChildren[i];
        int nRet = read(pChild);
        if (nRet != ERR_OK) {
            return nRet;
        }
    }

    // verifyData(m_fp, parent);

    return ERR_OK;
}

M4aBox *M4aTag::getBox(cstr_t szPath) {
    return MediaTags::getBox(&m_root, szPath);
}

int M4aTag::listLyrics(VecStrings &vLyrNames) {
    if (hasLyrics()) {
        vLyrNames.push_back(SZ_SONG_M4A_LYRICS);
    }

    return ERR_OK;
}

int M4aTag::getLyrics(string &strLyrics) {
    M4aBox *box = getBox("moov.udta.meta.ilst.\xA9lyr");
    if (!box) {
        return ERR_NOT_FOUND;
    }

    return box->getDataUtf8String(strLyrics);
}

bool M4aTag::hasLyrics() {
    return getBox("moov.udta.meta.ilst.\xA9lyr") != nullptr;
}

int M4aTag::setLyrics(const string &lyrics) {
    M4aBox *parent = getBox("moov.udta.meta.ilst");
    if (parent == nullptr) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    getBox("moov")->m_nSizeNew = 0;

    M4aBox *box = parent->findBox(SZ_TYPE_LYRICS);
    if (box == nullptr) {
        box = parent->addNewBox(SZ_TYPE_LYRICS);
    }

    box->setDataUtf8String(lyrics);

    return ERR_OK;
}

int M4aTag::removeLyrics() {
    M4aBox *parent = getBox("moov.udta.meta.ilst");
    if (parent == nullptr) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    getBox("moov")->m_nSizeNew = 0;

    parent->deleteChild(SZ_TYPE_LYRICS);

    return ERR_OK;
}

int M4aTag::setPictures(const VecStrings &pictures) {
    M4aBox *box = getBox("moov.udta.meta.ilst");
    if (box == nullptr) {
        return ERR_NOT_SUPPORT_FILE_FORMAT;
    }

    getBox("moov")->m_nSizeNew = 0;

    auto &children = box->m_vChildren;
    bool isAdded = pictures.empty();
    for (auto it = children.begin(); it != children.end();) {
        auto child = *it;
        if (child->isType(SZ_TYPE_PICTRUE)) {
            if (isAdded) {
                // 删除多的
                it = children.erase(it);
                continue;
            } else {
                child->setDataPictures(pictures);
                isAdded = true;
            }
        }
        ++it;
    }

    if (!isAdded) {
        // 添加
        auto child = box->addNewBox(SZ_TYPE_PICTRUE);
        child->setDataPictures(pictures);
    }

    return ERR_OK;
}

VecStrings M4aTag::getPictures() {
    M4aBox *box = getBox("moov.udta.meta.ilst");
    if (!box) {
        return {};
    }

    VecStrings pics;
    for (auto child : box->m_vChildren) {
        if (child->isType(SZ_TYPE_PICTRUE)) {
            child->getDataPictures(pics);
        }
    }

    return pics;
}

int M4aTag::write(string &buf, size_t offset) {
    fseek(m_fp, offset, SEEK_SET);
    if (fwrite(buf.c_str(), 1, buf.size(), m_fp) != buf.size()) {
        return ERR_WRITE_FILE;
    }
    return ERR_OK;
}


// 返回支持的媒体文件扩展名，不包含 '.'
VecStrings M4aFileTags::getSupportedExtNames() {
    return {"m4a", "mp4"};
}

VecStrings M4aFileTags::getSupportedSavingLyricsUrls() {
    return { SZ_SONG_M4A_LYRICS };
}

string M4aFileTags::getFileKind(const StringView &fileExt) {
    if (fileExt.iEqual("m4a")) {
        return "MPEG-4 Audio (m4a)";
    } else {
        return "MPEG-4 Audio and Video (mp4)";
    }
}

int M4aFileTags::getTags(cstr_t fileName, BasicMediaTags &tagsOut, ExtendedMediaInfo &extendedInfoOut) {
    M4aTag tag;
    int ret = tag.open(fileName, false);
    if (ret != ERR_OK) {
        return ret;
    }

    tag.getTags(tagsOut);
    extendedInfoOut = tag.getExtMediaInfo();

    return ERR_OK;
}

int M4aFileTags::setBasicTags(cstr_t fileName, const BasicMediaTags &tags) {
    M4aTag tag;
    int ret = tag.open(fileName, true);
    if (ret != ERR_OK) {
        return ret;
    }

    tag.setTags(tags);

    return tag.save();
}

VecStrings M4aFileTags::enumLyrics(cstr_t fileName) {
    M4aTag tag;
    int ret = tag.open(fileName, false);
    if (ret == ERR_OK) {
        string strLyrics;
        ret = tag.getLyrics(strLyrics);
        if (ret == ERR_OK) {
            return { SZ_SONG_M4A_LYRICS };
        }
    }

    return {};
}

int M4aFileTags::getLyrics(cstr_t fileName, cstr_t lyricsUrl, bool isUseSpecifiedEncoding, CharEncodingType encodingSpecified, RawLyrics &rawLyricsOut) {
    M4aTag tag;
    int ret = tag.open(fileName, false);
    if (ret == ERR_OK) {
        string strLyrics;
        ret = tag.getLyrics(strLyrics);
        if (ret == ERR_OK) {
            return parseLyricsBinary(strLyrics, isUseSpecifiedEncoding, encodingSpecified, rawLyricsOut);
        }
    }

    return ret;
}

int M4aFileTags::setLyrics(cstr_t fileName, const RawLyrics &rawLyrics) {
    M4aTag tag;
    int ret = tag.open(fileName, true);
    if (ret == ERR_OK) {
        auto lyrics = toLyricsString(rawLyrics);
        tag.setLyrics(lyrics);
        tag.save();
    }

    return ret;
}

int M4aFileTags::setLyrics(cstr_t fileName, const VecStrings &lyricsUrls, const RawLyrics &rawLyrics) {
    for (StringView url : lyricsUrls) {
        if (url.equal(SZ_SONG_M4A_LYRICS)) {
            return setLyrics(fileName, rawLyrics);
        }
    }

    return ERR_NOT_SUPPORT_FILE_FORMAT;
}

int M4aFileTags::removeLyrics(cstr_t fileName, const VecStrings &lyrUrlsToRemove) {
    for (StringView url : lyrUrlsToRemove) {
        if (url.equal(SZ_SONG_M4A_LYRICS)) {
            M4aTag tag;
            int ret = tag.open(fileName, true);
            if (ret == ERR_OK) {
                tag.removeLyrics();
                tag.save();
            }

            return ret;
        }
    }

    return ERR_NOT_SUPPORT_FILE_FORMAT;
}

int M4aFileTags::getPicture(cstr_t fileName, uint32_t index, string &imageDataOut) {
    M4aTag tag;
    int ret = tag.open(fileName, false);
    if (ret != ERR_OK) {
        return ret;
    }

    auto pics = tag.getPictures();
    if (index >= pics.size()) {
        return ERR_NOT_FOUND;
    }

    imageDataOut = pics[index];
    return ERR_OK;
}

void M4aFileTags::getPictures(cstr_t fileName, VecStrings &vImagesDataOut) {
    M4aTag tag;
    int ret = tag.open(fileName, false);
    if (ret == ERR_OK) {
        vImagesDataOut = tag.getPictures();
    }
}

int M4aFileTags::setPictures(cstr_t fileName, const VecStrings &picturesData) {
    M4aTag tag;
    int ret = tag.open(fileName, true);
    if (ret == ERR_OK) {
        tag.setPictures(picturesData);
        tag.save();
    }

    return ret;
}

} // namespace MediaTags
