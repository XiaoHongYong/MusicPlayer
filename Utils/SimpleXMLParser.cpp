#include "UtilsTypes.h"
#include "Utils.h"
#include "SimpleXMLParser.h"


//
// return CharEncodingType of XML file.
//
CharEncodingType getXMLTxtEncoding(const char *szTxt, int nLen) {
    // encoding='gb2312'
    const char *szEncoding = "encoding";
    const char *pE;
    const char *szEnd = szTxt + (nLen < 150 ? nLen : 150);
    char szCoding[64];
    char *p;

    while (szTxt < szEnd) {
        if (*szTxt == 'e') {
            // 12 = strlen(encoding=' ')
            // make sure text is long enough.
            if (szTxt + 12 >= szEnd) {
                return ED_SYSDEF;
            }

            // eat up "encoding"
            pE = szEncoding;
            while (*pE && *szTxt == *pE) {
                pE++;
                szTxt++;
            }

            if (*pE == '\0') {
                // ignore space
                while (*szTxt == ' ') {
                    szTxt++;
                }

                // eat up =' or ="
                if (*szTxt == '=' && (szTxt[1] == '\'' || szTxt[1] == '\"')) {
                    szTxt += 2;
                    // max encoding string length is 32.
                    if (szEnd > szTxt + 32) {
                        szEnd = szTxt + 32;
                    }
                    p = szCoding;
                    while (szTxt < szEnd && *szTxt != '\'' && *szTxt != '\"') {
                        *p = *szTxt;
                        p++;
                        szTxt++;
                    }
                    *p = '\0';
                    return getCharEncodingID(szCoding);
                }
            }

        }
        szTxt++;
    }

    return ED_SYSDEF;
}



CSimpleXMLParser::CSimpleXMLParser() {
    m_szData = nullptr;
    m_nDeepth = 0;
    m_nLen = 0;
    nPos = 0;
    nPosNext = 0;
    m_encodingID = ED_SYSDEF;
    m_bNoProlog = false;
}

CSimpleXMLParser::~CSimpleXMLParser() {
}


//
// UTF8: EF BB BF
// UNICODE: FF FE
// UNICODE 编码和其它编码的解析方式不同，需要单独处理
SimpleXMLParserError CSimpleXMLParser::parseData(const void *lpXmlData, size_t nLen, CSimpleXML &xmlData) {
    int bomSize = 0;
    uint8_t *szBuffer = (uint8_t *)lpXmlData;

    CharEncodingType encoding = detectFileEncoding(szBuffer, nLen, bomSize);

    szBuffer += bomSize;
    nLen -= bomSize;

    if (encoding == ED_UNICODE || encoding == ED_UNICODE_BIG_ENDIAN) {
        string str;
        if (encoding == ED_UNICODE_BIG_ENDIAN) {
            ucs2EncodingReverse((utf16_t *)szBuffer, (int)nLen / sizeof(utf16_t));
        }
        ucs2ToUtf8((const utf16_t *)szBuffer, (int)nLen / sizeof(utf16_t), str);

        return doParse(str.c_str(), (int)str.size(), xmlData);
    } else {
        return doParse((const char *)szBuffer, (int)nLen, xmlData);
    }
}

void CSimpleXMLParser::ignoreSpace(int nPos, int &nPosNext) {
    nPosNext = nPos;
    while (m_szData[nPosNext] == 0x20 || m_szData[nPosNext] == 0x9 ||
        m_szData[nPosNext] == 0xD || m_szData[nPosNext] == 0xA) {
        nPosNext++;
    }
}

#define SZ_L_XML_DECL       "<?xml"
#define LEN_L_XML_DECL      5

#define SZ_CDATA_START      "<![CDATA["
#define LEN_CDATA_START     9

#define SZ_CDATA_END        "]]>"
#define LEN_CDATA_END       3

#define SZ_COMMENTS_START   "<!--"
#define LEN_COMMENTS_START  4

#define SZ_COMMENTS_END     "-->"
#define LEN_COMMENTS_END    3

cstr_t __szEsc[] = { "lt;", "gt;", "amp;", "quot;", "apos;", nullptr };
int __nLen[] = { 3, 3, 4, 5, 5 };
char __chOrg[] = { '<', '>', '&', '\"', '\'' };

ElementType CSimpleXMLParser::readElemnt(ElementType desireType, string &strElemnt, int nPos, int &nPosNext) {
    ElementType retType = ET_UNKNOWN;
    int k;

    if (nPos >= m_nLen) {
        return ET_EOF_FILE;
    }

    // <?xml version="1.0" encoding='gb2312'?>
    switch (desireType) {
    case ET_EQ:        // =
        if (m_szData[nPos] == '=') {
            retType = desireType;
            nPosNext = nPos + 1;
        }
        break;
    case ET_L_BRACKET:        // <
        if (m_szData[nPos] == '<') {
            retType = desireType;
            nPosNext = nPos + 1;
        }
        break;
    case ET_CDATA:        // <![CDATA[
        retType = ET_UNKNOWN;
        if (strncmp(m_szData + nPos, SZ_CDATA_START, LEN_CDATA_START) == 0) {
            nPos += LEN_CDATA_START;
            for (nPosNext = nPos; nPosNext + 2 < m_nLen; nPosNext++) {
                if (m_szData[nPosNext] == ']' && m_szData[nPosNext + 1] == ']' && m_szData[nPosNext + 2] == '>') {
                    // Found end tag of CDATA.
                    strElemnt.clear();
                    strElemnt.append(m_szData + nPos, nPosNext - nPos);
                    nPosNext += LEN_CDATA_END;
                    retType = desireType;
                    break;
                }
            }
        }
        break;
    case ET_COMMENTS:        // <!-- -->
        retType = ET_UNKNOWN;
        if (strncmp(m_szData + nPos, SZ_COMMENTS_START, LEN_COMMENTS_START) == 0) {
            nPos += LEN_COMMENTS_START;
            for (nPosNext = nPos; nPosNext + 2 < m_nLen; nPosNext++) {
                if (m_szData[nPosNext] == '-' && m_szData[nPosNext + 1] == '-' && m_szData[nPosNext + 2] == '>') {
                    // Found end tag of COMMENTS.
                    strElemnt.clear();
                    strElemnt.append(m_szData + nPos, nPosNext - nPos);
                    nPosNext += LEN_COMMENTS_END;
                    retType = desireType;
                    break;
                }
            }
        }
        break;
    case ET_R_BRACKET:        // >
        if (m_szData[nPos] == '>') {
            retType = desireType;
            nPosNext = nPos + 1;
        }
        break;
    case ET_EL_BRACKET:        // </
        if (m_szData[nPos] == '<' && m_szData[nPos + 1] == '/') {
            retType = desireType;
            nPosNext = nPos + 2;
        }
        break;
    case ET_ER_BRACKET:        // />
        if (m_szData[nPos] == '/' && m_szData[nPos + 1] == '>') {
            retType = desireType;
            nPosNext = nPos + 2;
        }
        break;
    case ET_L_XML_DECL:
        // <?xml
        if (strncmp(m_szData + nPos, SZ_L_XML_DECL, LEN_L_XML_DECL) == 0) {
            retType = ET_L_XML_DECL;
            nPosNext = nPos + LEN_L_XML_DECL;
        }
        break;
    case ET_R_XML_DECL:        // ?>
        if (m_szData[nPos] == '?' && m_szData[nPos + 1] == '>') {
            retType = desireType;
            nPosNext = nPos + 2;
        }
        break;
    case ET_SPACE:
        // White Space
        // [3]    S    ::=    (#x20 | #x9 | #xD | #xA)+
        {
            int i = nPos;
            strElemnt.clear();
            while (m_szData[i] == 0x20 || m_szData[i] == 0x9 ||
                m_szData[i] == 0xD || m_szData[i] == 0xA) {
                strElemnt += m_szData[i];
                i++;
            }
            if (i > nPos) {
                retType = ET_SPACE;
                nPosNext = i;
            }
        }
        break;
    case ET_NAME:
        // Names and Tokens
        //    [4]    NameChar    ::=    Letter | Digit | '.' | '-' | '_' | ':' | CombiningChar | Extender
        //    [5]    Name    ::=    (Letter | '_' | ':') (NameChar)*
        //    [6]    Names    ::=    Name (S Name)*
        //    [7]    Nmtoken    ::=    (NameChar)+
        //    [8]    Nmtokens    ::=    Nmtoken (S Nmtoken)*
        {
            int i = nPos;
            strElemnt.clear();

            if (isAlpha(m_szData[i]) || m_szData[i] == '_') {
                strElemnt += m_szData[i];
                i++;
                while (isAlpha(m_szData[i]) || isDigit(m_szData[i]) || m_szData[i] == '_'
                    || m_szData[i] == '-' || m_szData[i] == '.' || m_szData[i] == ':') {
                    strElemnt += m_szData[i];
                    i++;
                }
            }
            if (i > nPos) {
                retType = ET_NAME;
                nPosNext = i;
            }
        }
        break;
    case ET_VALUE:
        {
            // "value"
            int i = nPos;
            strElemnt.clear();
            char chTag;

            chTag = m_szData[i];
            if (chTag == '\"' || chTag == '\'') {
                i++;
                while (m_szData[i] != chTag && i < m_nLen) {
                    if (m_szData[i] == '&') {
                        i++;
                        for (k = 0; __szEsc[k] != nullptr; k++) {
                            if (i + __nLen[k] < m_nLen && strncmp(m_szData + i, __szEsc[k], __nLen[k]) == 0) {
                                strElemnt += __chOrg[k];
                                i += __nLen[k];
                                break;
                            }
                        }
                        if (__szEsc[k] == nullptr) {
                            strElemnt += '&';
                        }
                    } else {
                        strElemnt += m_szData[i];
                        i++;
                    }
                }
                if (m_szData[i] == chTag) {
                    retType = ET_VALUE;
                    nPosNext = i + 1;
                }
            }
        }
        break;
    case ET_TEXT_VALUE:
        {
            //
            int i = nPos;
            strElemnt.clear();

            while (m_szData[i] != '<' && i < m_nLen) {
                if (m_szData[i] == '&') {
                    i++;
                    for (k = 0; __szEsc[k] != nullptr; k++) {
                        if (i + __nLen[k] < m_nLen && strncmp(m_szData + i, __szEsc[k], __nLen[k]) == 0) {
                            strElemnt += __chOrg[k];
                            i += __nLen[k];
                            break;
                        }
                    }
                    if (__szEsc[k] == nullptr) {
                        strElemnt += m_szData[i];
                    }
                } else {
                    strElemnt += m_szData[i];
                    i++;
                }
            }
            if (m_szData[i] == '<' && i > nPos) {
                retType = ET_TEXT_VALUE;
                nPosNext = i;
            }
        }
        break;
    case ET_ANY:
        {
            int i = nPos;
            strElemnt.clear();

            if (m_szData[i] == '\"') {
                return readElemnt(ET_VALUE, strElemnt, nPos, nPosNext);
            } else if (m_szData[i] == 0x20) {
                return readElemnt(ET_SPACE, strElemnt, nPos, nPosNext);
            } else if (m_szData[i] == '=') {
                return readElemnt(ET_EQ, strElemnt, nPos, nPosNext);
            } else if (m_szData[i] == '<') {
                if (m_szData[i + 1] == '/') {
                    return readElemnt(ET_EL_BRACKET, strElemnt, nPos, nPosNext);
                } else if (m_szData[i + 1] == '!' && m_szData[i + 2] == '[') {
                    ElementType etRet = readElemnt(ET_CDATA, strElemnt, nPos, nPosNext);
                    if (etRet == ET_CDATA) {
                        return ET_TEXT_VALUE; // Change CData to ET_VALUE.
                    } else {
                        return etRet;
                    }
                } else if (m_szData[i + 1] == '!' && m_szData[i + 2] == '-' && m_szData[i + 3] == '-') {
                    return readElemnt(ET_COMMENTS, strElemnt, nPos, nPosNext);
                } else {
                    return readElemnt(ET_L_BRACKET, strElemnt, nPos, nPosNext);
                }
            } else if (m_szData[i] == '>') {
                return readElemnt(ET_R_BRACKET, strElemnt, nPos, nPosNext);
            } else if (m_szData[i] == '?') {
                if (m_szData[i + 1] == '>') {
                    return readElemnt(ET_R_XML_DECL, strElemnt, nPos, nPosNext);
                }
            } else if (m_szData[i] == '/') {
                if (m_szData[i + 1] == '>') {
                    return readElemnt(ET_ER_BRACKET, strElemnt, nPos, nPosNext);
                }
            } else if (isAlpha(m_szData[i])) {
                // name?
                return readElemnt(ET_NAME, strElemnt, nPos, nPosNext);
            }
        }
        break;
    default:
        break;
    }
    return retType;
}

SimpleXMLParserError CSimpleXMLParser::doParse(const char *szData, int nLen, CSimpleXML &xmlData) {
    SimpleXMLParserError Error;

    m_szData = szData;
    m_nLen = nLen;

    nPos = 0; nPosNext = 0;
    Error = parseProlog(nPos, nPosNext, xmlData);
    if (Error != SXPE_OK) {
        return Error;
    }

    SXNode node;

    nPos = nPosNext;
    // XML 允许 prolog 与根元素之间有 Misc（注释 / 空白 / PI）。
    // 以前只 ignoreSpace，遇到 <!-- 会把 '<' 当成根标签起点，导致解析失败或错乱。
    while (1) {
        ignoreSpace(nPos, nPosNext);
        nPos = nPosNext;
        string strMisc;
        ElementType miscType = readElemnt(ET_COMMENTS, strMisc, nPos, nPosNext);
        if (miscType == ET_COMMENTS) {
            nPos = nPosNext;
            continue;
        }
        break;
    }

    m_nDeepth = 0;
    // parse root entity
    Error = parseEntity(nPos, nPosNext, &node);
    if (Error != SXPE_OK) {
        return Error;
    }
    nPos = nPosNext;

    // Ignore space
    ignoreSpace(nPos, nPosNext);
    nPos = nPosNext;

    // Make sure there's no other elements
    string strElement;
    ElementType desireType = ET_EOF_FILE;
    ElementType retType = readElemnt(desireType, strElement, nPos, nPosNext);
    if (retType != desireType) {
        return SXPE_ONE_ROOT_ENTITY_ALLOWED;
    }

    assert(node.listChildren.size() == 1);
    if (node.listChildren.size() != 1) {
        return SXPE_FORMAT_ERROR;
    }

    xmlData.m_pRoot = node.listChildren.front();
    node.listChildren.clear();

    return SXPE_OK;
}

SimpleXMLParserError CSimpleXMLParser::parseProlog(int nPos, int &nPosNext, CSimpleXML &xmlData) {
    string strElement;
    ElementType retType;
    ElementType desireType;
    string name;
    string strValue;

    nPos = 0; nPosNext = 0;

    // ignore all blank spaces..
    readElemnt(ET_SPACE, name, nPos, nPosNext);
    nPos = nPosNext;

    if (ET_L_XML_DECL != readElemnt(ET_L_XML_DECL, strElement, nPos, nPosNext)) {
        // 不是XML文件
        if (m_bNoProlog) {
            // No prologue
            nPosNext = 0;
            m_encodingID = ED_UTF8;
            return SXPE_OK;
        }

        return SXPE_NOT_XML_FORMAT;
    }
    m_bNoProlog = false;

    nPos = nPosNext;

    desireType = ET_SPACE;
    retType = readElemnt(desireType, strElement, nPos, nPosNext);
    if (retType != desireType) {
        return SXPE_FORMAT_ERROR;
    }
    nPos = nPosNext;

    // 读入XML declaration
    while (1) {
        // name
        desireType = ET_NAME;
        retType = readElemnt(desireType, name, nPos, nPosNext);
        if (retType != desireType) {
            return SXPE_FORMAT_ERROR;
        }
        nPos = nPosNext;

        ignoreSpace(nPos, nPosNext);
        nPos = nPosNext;

        desireType = ET_EQ;
        retType = readElemnt(desireType, strElement, nPos, nPosNext);
        if (retType != ET_EQ) {
            return SXPE_FORMAT_ERROR;
        }
        nPos = nPosNext;

        ignoreSpace(nPos, nPosNext);
        nPos = nPosNext;

        desireType = ET_VALUE;
        retType = readElemnt(desireType, strValue, nPos, nPosNext);
        if (retType != desireType) {
            return SXPE_FORMAT_ERROR;
        }
        nPos = nPosNext;

        if (name == "encoding") {
            // EncodingDecl    ::=    S 'encoding' Eq ('"' EncName '"' | "'" EncName "'" )
            // EncName    ::=    [A-Za-z] ([A-Za-z0-9._] | '-')* /* Encoding name contains only Latin characters */
            xmlData.m_encoding = strValue.c_str();
            m_encodingID = getCharEncodingID(xmlData.m_encoding.c_str());
        }

        ignoreSpace(nPos, nPosNext);
        nPos = nPosNext;

        desireType = ET_R_XML_DECL;
        retType = readElemnt(desireType, strValue, nPos, nPosNext);
        if (retType == desireType) {
            break;
        }
    }

    return SXPE_OK;
}

SimpleXMLParserError CSimpleXMLParser::parseEntity(int nPos, int &nPosNext, SXNode *pNode) {
    string strElement;
    SimpleXMLParserError Error;

    CDeepStack deepstack(&m_nDeepth);
    string strNodeName;
    string strPropName, strValue;

    ElementType retType;
    ElementType desireType;
    SXNode *pNewNode;

    ignoreSpace(nPos, nPosNext);
    nPos = nPosNext;

    // <
    desireType = ET_L_BRACKET;
    retType = readElemnt(desireType, strElement, nPos, nPosNext);
    if (retType != desireType) {
        return SXPE_FORMAT_ERROR;
    }
    nPos = nPosNext;

    // <name
    desireType = ET_NAME;
    retType = readElemnt(desireType, strNodeName, nPos, nPosNext);
    if (retType != desireType) {
        return SXPE_FORMAT_ERROR;
    }
    nPos = nPosNext;

    pNewNode = new SXNode;
    pNewNode->name = strNodeName.c_str();
    pNode->listChildren.push_back(pNewNode);

    // <name property, or <name> or <name/>
    desireType = ET_ANY;
    retType = readElemnt(desireType, strElement, nPos, nPosNext);
    nPos = nPosNext;
    if (retType == ET_SPACE) {
        // <name property
        while (1) {
            ignoreSpace(nPos, nPosNext);
            nPos = nPosNext;

            desireType = ET_NAME;
            retType = readElemnt(desireType, strPropName, nPos, nPosNext);
            if (retType != desireType) {
                return SXPE_FORMAT_ERROR;
            }
            nPos = nPosNext;

            ignoreSpace(nPos, nPosNext);
            nPos = nPosNext;

            desireType = ET_EQ;
            retType = readElemnt(desireType, strElement, nPos, nPosNext);
            if (retType != desireType) {
                return SXPE_FORMAT_ERROR;
            }
            nPos = nPosNext;

            ignoreSpace(nPos, nPosNext);
            nPos = nPosNext;

            // value
            desireType = ET_VALUE;
            retType = readElemnt(desireType, strValue, nPos, nPosNext);
            if (retType != desireType) {
                return SXPE_FORMAT_ERROR;
            }
            nPos = nPosNext;

            pNewNode->addProperty(strPropName.c_str(), strValue.c_str());

            ignoreSpace(nPos, nPosNext);
            nPos = nPosNext;

            // />
            desireType = ET_ER_BRACKET;
            retType = readElemnt(desireType, strValue, nPos, nPosNext);
            if (retType == desireType) {
                nPos = nPosNext;
                break;
            }

            // ><...></name>
            desireType = ET_R_BRACKET;
            retType = readElemnt(desireType, strValue, nPos, nPosNext);
            if (retType == desireType) {
                // break
                nPos = nPosNext;
                break;
            }
        }
    }

    if (retType == ET_ER_BRACKET) {
        // <name/>
        // no property, no children
        return SXPE_OK;
    } else if (retType == ET_R_BRACKET) {
        // Has value and children

        // value
        desireType = ET_TEXT_VALUE;
        retType = readElemnt(desireType, strElement, nPos, nPosNext);
        if (retType == ET_TEXT_VALUE) {
            nPos = nPosNext;
            pNewNode->strContent = strElement.c_str();
            trimStr(pNewNode->strContent);
        }

        while (1) {
            ignoreSpace(nPos, nPosNext);
            nPos = nPosNext;

            // parse all child values, entities, etc.
            desireType = ET_ANY;
            retType = readElemnt(desireType, strElement, nPos, nPosNext);
            if (retType == ET_TEXT_VALUE) {
                pNewNode->strContent += strElement.c_str();
            } else if (retType == ET_COMMENTS) {
                // Just ignore comments
            } else if (retType == ET_L_BRACKET) {
                Error = parseEntity(nPos, nPosNext, pNewNode);
                if (Error != SXPE_OK) {
                    return Error;
                }
            } else {
                nPos = nPosNext;
                break;
            }
            nPos = nPosNext;
        }

        // End of Entity: </name>
        if (retType != ET_EL_BRACKET) {
            return SXPE_FORMAT_ERROR;
        }

        desireType = ET_NAME;
        retType = readElemnt(desireType, strElement, nPos, nPosNext);
        if (retType != desireType) {
            return SXPE_FORMAT_ERROR;
        }
        nPos = nPosNext;

        if (strElement != strNodeName) {
            return SXPE_FORMAT_ERROR;
        }

        desireType = ET_R_BRACKET;
        retType = readElemnt(desireType, strElement, nPos, nPosNext);
        if (retType != desireType) {
            return SXPE_FORMAT_ERROR;
        }

        return SXPE_OK;
    }

    ignoreSpace(nPos, nPosNext);
    nPos = nPosNext;

    // </
    desireType = ET_EL_BRACKET;
    retType = readElemnt(desireType, strElement, nPos, nPosNext);
    if (retType == desireType) {
        return SXPE_FORMAT_ERROR;
    }

    return SXPE_OK;
}
