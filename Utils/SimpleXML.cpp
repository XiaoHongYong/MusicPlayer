#include "Utils.h"
#include "SimpleXML.h"
#include "SimpleXMLParser.h"
#include "MLBinXMLParser.h"
#include "XMLWriter.h"


SXNode *dupSXNode(SXNode *pOrgNode) {
    SXNode *pNewNode;

    pNewNode = new SXNode;
    assert(pNewNode);
    if (!pNewNode) {
        return nullptr;
    }

    for (SXNode::iterator it = pOrgNode->listChildren.begin();
    it != pOrgNode->listChildren.end(); ++it)
        {
        SXNode *pChild = dupSXNode(*it);
        pNewNode->listChildren.push_back(pChild);
    }

    pNewNode->name = pOrgNode->name;
    pNewNode->strContent = pOrgNode->strContent;

    pNewNode->listProperties.insert(pNewNode->listProperties.begin(),
        pOrgNode->listProperties.begin(), pOrgNode->listProperties.end());

    return pNewNode;
}

SXNode::~SXNode() {
    iterator it, itEnd;

    itEnd = listChildren.end();
    for (it = listChildren.begin(); it != itEnd; ++it) {
        SXNode *p = *it;
        delete p;
    }
    listChildren.clear();

    listProperties.clear();
}

SXNode::ListProperties::iterator SXNode::FindProperty(cstr_t szPropName) {
    ListProperties::iterator it, itEnd;

    itEnd = listProperties.end();

    for (it = listProperties.begin(); it != itEnd; ++it) {
        Property &prop = *it;
        if (strcmp(szPropName, prop.name.c_str()) == 0) {
            return it;
        }
    }

    return listProperties.end();
}

// If not exists, nullptr will be returned
cstr_t SXNode::getProperty(cstr_t szPropName) {
    ListProperties::iterator it = FindProperty(szPropName);
    if (it == listProperties.end()) {
        return nullptr;
    }

    return (*it).strValue.c_str();
}

int SXNode::getPropertyInt(cstr_t szPropName) {
    ListProperties::iterator it = FindProperty(szPropName);
    if (it == listProperties.end()) {
        return 0;
    }

    return atoi((*it).strValue.c_str());
}

int SXNode::getPropertyInt(cstr_t szPropName, int nValueIfInexist) {
    ListProperties::iterator it = FindProperty(szPropName);
    if (it == listProperties.end()) {
        return nValueIfInexist;
    }

    return atoi((*it).strValue.c_str());
}

// If not exists, "" will be returned.
cstr_t SXNode::getPropertySafe(cstr_t szPropName) {
    cstr_t szValue;

    szValue = getProperty(szPropName);
    if (szValue) {
        return szValue;
    } else {
        return "";
    }
}

bool SXNode::getPropertyBinData(cstr_t szPropName, string &buf) {
    ListProperties::iterator it = FindProperty(szPropName);
    if (it == listProperties.end()) {
        return false;
    }

    Property &prop = *it;
    buf = prop.strValue;
    return true;
}

bool SXNode::getPropertyBase64Data(cstr_t szPropName, string &buf) {
    ListProperties::iterator it = FindProperty(szPropName);
    if (it == listProperties.end()) {
        return false;
    }

    Property &prop = *it;
    buf = base64Decode(prop.strValue.c_str(), prop.strValue.size());
    return true;
}

void SXNode::addProperty(cstr_t szPropName, cstr_t szPropValue) {
    assert(FindProperty(szPropName) == listProperties.end());

    Property prop;
    prop.name = szPropName;
    prop.strValue = szPropValue;
    listProperties.push_back(prop);
}

void SXNode::setProperty(cstr_t szPropName, cstr_t szPropValue) {
    ListProperties::iterator it = FindProperty(szPropName);
    if (it == listProperties.end()) {
        addProperty(szPropName, szPropValue);
    } else {
        Property &prop = *it;
        prop.strValue = szPropValue;
    }
}

void SXNode::eraseProperty(cstr_t szPropName) {
    ListProperties::iterator it = FindProperty(szPropName);
    if (it != listProperties.end()) {
        listProperties.erase(it);
    }
}

// get first child same with the node name
SXNode *SXNode::getChild(cstr_t szName) {
    iterator it, itEnd;

    itEnd = listChildren.end();
    for (it = listChildren.begin(); it != itEnd; ++it) {
        SXNode *p = *it;
        if (strcmp(p->name.c_str(), szName) == 0) {
            return p;
        }
    }

    return nullptr;
}

void SXNode::toXML(CXMLWriter &xmlStream) {
    xmlStream.writeStartElement(name.c_str());

    // write properties
    {
        ListProperties::iterator it, itEnd;

        itEnd = listProperties.end();
        for (it = listProperties.begin(); it != itEnd; ++it) {
            Property &prop = *it;
            xmlStream.writeAttribute(prop.name.c_str(), prop.strValue.c_str());
        }
    }

    // write content
    if (!strContent.empty()) {
        xmlStream.writeString(strContent.c_str());
    }

    // write children
    {
        iterator it, itEnd;

        itEnd = listChildren.end();
        for (it = listChildren.begin(); it != itEnd; ++it) {
            SXNode *pNode = *it;
            pNode->toXML(xmlStream);
        }
    }

    xmlStream.writeEndElement();
}

#ifdef _DEBUG
/*void PrintNode(SXNode *node, int &nDeep, string &str)
{
    nDeep++;

    str += "<";
    str += node->getName();
    str += ">";

    str += node->GetContent();

    for (list<SXNode *>::iterator it = node->listChildren.begin();
        it != node->listChild.end(); ++it)
    {
        printNode(*it, nDeep, str);
    }
    if (node->type == SX_NODE_ELEMENT)
    {
        str += "</";
        str += node->name;
        str += ">\r\n";
    }
    nDeep--;
}*/
#endif



CSimpleXML::CSimpleXML() {
    m_pRoot = nullptr;
}

CSimpleXML::~CSimpleXML() {
    free();
}

void CSimpleXML::New(cstr_t szRootName) {
    free();

    m_pRoot = new SXNode;
    m_pRoot->name = szRootName;
}

bool CSimpleXML::parseFile(cstr_t szXmlFile, bool bNoProlog) {
    string buf;

    if (!readFile(szXmlFile, buf)) {
        return false;
    }

    return parseData(buf.data(), buf.size(), bNoProlog);
}

bool CSimpleXML::parseData(const void *lpXmlData, size_t nLen, bool bNoProlog) {
    free();

    if (CMLBinXMLParser::isHeaderType((char *)lpXmlData)) {
        CMLBinXMLParser parser;
        CMLBinXMLParser::MLBinXmlParseError ret;

        ret = parser.parseData(lpXmlData, nLen, *this);
        return (ret == CMLBinXMLParser::E_OK);
    } else {
        CSimpleXMLParser parser;
        SimpleXMLParserError ret;

        parser.m_bNoProlog = bNoProlog;
        ret = parser.parseData(lpXmlData, nLen, *this);
        return (ret == SXPE_OK);
    }
}

bool CSimpleXML::saveFile(cstr_t szXmlFile) {
    if (!m_pRoot) {
        return false;
    }

    CXMLWriter xmlStream(m_encoding.empty());

    if (!m_encoding.empty()) {
        xmlStream.writeStartDocument(m_encoding.c_str());
    }

    m_pRoot->toXML(xmlStream);

    xmlStream.writeEndDocument();

    return xmlStream.saveAsFile(szXmlFile);
}

void CSimpleXML::free() {
    if (m_pRoot) {
        delete m_pRoot;
        m_pRoot = nullptr;
    }
}

#if UNIT_TEST

#include "utils/unittest.h"


TEST(SimpleXML, ParseCDATA) {
    CSimpleXML xml;
    cstr_t SZ_XML = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
    "<root><![CDATA[value]]><![CDATA[value]]></root>";

    ASSERT_TRUE(xml.parseData(SZ_XML, strlen(SZ_XML)));
    ASSERT_TRUE(xml.m_pRoot->name == "root");
    ASSERT_TRUE(xml.m_pRoot->strContent == "valuevalue");
}

TEST(SimpleXML, ParseCommentBetweenPrologAndRoot) {
    // 浮动歌词等皮肤 XML 常在 <?xml?> 后写注释；必须能正确跳过。
    CSimpleXML xml;
    cstr_t SZ_XML = "<?xml version=\"1.0\" encoding='UTF-8'?>\n"
        "<!-- comment with 中文 and\n"
        "     multiple lines -->\n"
        "<skinwnd Name=\"floatinglyrwnd\" BgColor=\"#1C1C38\">\n"
        "  <LyricsShow OutlineLyrText=\"TRUE\"/>\n"
        "</skinwnd>";

    ASSERT_TRUE(xml.parseData(SZ_XML, strlen(SZ_XML)));
    ASSERT_TRUE(xml.m_pRoot != nullptr);
    ASSERT_TRUE(xml.m_pRoot->name == "skinwnd");
    ASSERT_TRUE(strcmp(xml.m_pRoot->getPropertySafe("Name"), "floatinglyrwnd") == 0);
    ASSERT_TRUE(strcmp(xml.m_pRoot->getPropertySafe("BgColor"), "#1C1C38") == 0);
}

TEST(SimpleXML, SXNode) {
    SXNode node;

    cstr_t SZ_NODE_NAME = "node";
    cstr_t SZ_CONTENT_NAME = "content";
    cstr_t SZ_PROP_NAME = "PropName_%d";
    cstr_t SZ_VALUE_NAME = "Value_%d";
    int i;

    node.name = SZ_NODE_NAME;
    node.strContent = SZ_CONTENT_NAME;

    // add property
    for (i = 0; i < 5; i++) {
        char szName[64], szValue[64];
        cstr_t p;

        sprintf(szName, SZ_PROP_NAME, i);
        sprintf(szValue, SZ_VALUE_NAME, i);
        node.addProperty(szName, szValue);

        p = node.getProperty(szName);
        if (!p || !(strcmp(p, szValue) == 0)) {
            FAIL() << (stringPrintf("SXNode.getProperty(), case: %d, %s", i, szName).c_str());
        }
    }

    // Erase property
    for (i = 0; i < 1; i++) {
        char szName[64], szValue[64];
        cstr_t p;

        sprintf(szName, SZ_PROP_NAME, i);
        sprintf(szValue, SZ_VALUE_NAME, i);
        // node.addProperty(szName, szValue);

        node.eraseProperty(szName);
        p = node.getProperty(szName);
        if (p) {
            FAIL() << (stringPrintf("SXNode.eraseProperty(), case: %d, %s", i, szName).c_str());
        }
    }

    // To xml, and parse from xml
    CXMLWriter xmlWriter;
    CSimpleXML xml;

    node.toXML(xmlWriter);
    string &buf = xmlWriter.getBuffer();
    ASSERT_TRUE(xml.parseData(buf.c_str(), buf.size()));

    //
    // compare two xml data struct...
    //
    {
        // SXNode            *pNode;

        // ASSERT_TRUE(strcasecmp(pNode->name.c_str(), node.name.c_str()) == 0);
    }
}

#endif
