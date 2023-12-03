
#ifndef __XMLHELPER_H__
#define __XMLHELPER_H__

#include "tinyxml2.h"

#include <string>

using namespace tinyxml2;

namespace mediakit{

class CXmlHelper
{
public:
	CXmlHelper() {m_XmlDocument.Clear();}
	CXmlHelper(const char* pXml) {m_XmlDocument.Clear(); m_XmlDocument.Parse(pXml, strlen(pXml));}
	CXmlHelper(std::string& strXml) {m_XmlDocument.Clear(); m_XmlDocument.Parse(strXml.c_str(), strXml.size());}
	~CXmlHelper() {m_XmlDocument.Clear();}

public:
	typedef struct MessageHeader
	{
		std::string strGlobal;		// db33 Action or Response
		std::string strMethod;
		std::string strCmdType;
		std::string strDeviceID;
		uint64_t ulSN;
	}MessageHeader;

	typedef struct MessageRspHeader
	{
		uint64_t ulSN;
		std::string strCategory;
		std::string strCmdType;
		std::string strDeviceID;
		std::string strResult;
	}MessageRspHeader;

	bool prase();
	bool prase(const char* pXml);
	bool prase(std::string& strXml);

	std::string xmlMessage();

	bool buildKeepaliveXml(std::string& deviceId, uint64_t& ulSN);
	bool buildCatalogResponseXml(std::string channelId, std::string deviceId, uint64_t& ulSN);
public:
	bool analyzeHeader(MessageHeader& stMessageHeader);
	bool analyzeRspHeader(MessageRspHeader& stMessageRspHeader);

private:
	XMLElement* buildMessageHeader(const char* pMethod, const char* pCmdType, const char* pDeviceId, uint64_t ulSN);
	XMLElement* addNode(XMLNode* pXmlNode, const char* pName, const char* pValue);
	XMLElement* addNode(XMLNode* pXmlNode, const char* pName, int iValue);
	XMLElement* addNode(XMLNode* pXmlNode, const char* pName, uint64_t ulValue);

	XMLElement* lookupMessageValue(XMLElement* pElement, char const* pKey, std::string& strValue, const char* pDefaultValue = NULL);
	XMLElement* lookupMessageValue(XMLElement* pElement, char const* pKey, int& uiValue,int iDefaultValue = 0);
	XMLElement* lookupMessageValue(XMLElement* pElement, char const* pKey, unsigned int& iValue, unsigned int uiDefaultValue = 0);
	XMLElement* lookupMessageValue(XMLElement* pElement, char const* pKey, int64_t& ulValue, int64_t ulDefaultValue = 0);
	XMLElement* lookupMessageValue(XMLElement* pElement, char const* pKey);
	XMLElement* lookupMessageValue(XMLElement* pElement, char const* pKey, double& dValue, double dDefaultValue = 0.0);


private:
	tinyxml2::XMLDocument m_XmlDocument;
//	tinyxml2::XMLDeclaration m_XmlDeclaration;
};

}



#endif // __XMLHELPER_H__

