
#include "XmlHelper.h"

#include <stdlib.h>
#include <math.h>

namespace mediakit{

static int char2dec(char hex)
{
	if (hex >= '0' && hex <= '9'){
		return hex - '0';
	}
	else if ((hex >= 'a' && hex <= 'f')){
		return hex - 'a' + 10;
	}
	else if ((hex >= 'A' && hex <= 'F')){
		return hex - 'A' + 10;
	}
	return 0;
}

static long int hex2dec(char const* ptr, int n)
{
	long int decVal = 0;
	int i;
	int k = 0;
	for (i = n - 1; i >= 0; i--){
		long int val = char2dec(ptr[i]);
		decVal += (long int)(val * pow(16, k));
		k++;
	}
	return decVal;
}

std::string CXmlHelper::xmlMessage()
{
	XMLPrinter printer;
	m_XmlDocument.Print(&printer);
	return printer.CStr();
}

XMLElement* CXmlHelper::buildMessageHeader(const char* pMethod, const char* pCmdType, const char* pDeviceId, uint64_t ulSN)
{
	m_XmlDocument.Clear();
	XMLDeclaration* pDeclaration = m_XmlDocument.NewDeclaration();
	m_XmlDocument.InsertFirstChild(pDeclaration);

	XMLElement* pElement= m_XmlDocument.NewElement(pMethod);
	addNode(pElement, "CmdType", pCmdType);
	addNode(pElement, "SN", ulSN);
	addNode(pElement, "DeviceID", pDeviceId);
	m_XmlDocument.InsertEndChild(pElement);
	return pElement;
}

bool CXmlHelper::buildKeepaliveXml(std::string& deviceId, uint64_t& ulSN)
{
	XMLElement* pElement = buildMessageHeader("Notify", "Keepalive", deviceId.c_str(), ulSN);
	if (!pElement)
	{
		return false;
	}

	addNode(pElement, "Status", "OK");
	return true;
}

bool CXmlHelper::buildCatalogResponseXml(std::string channelId, std::string deviceId, uint64_t& ulSN)
{
	XMLElement* pElement = buildMessageHeader("Response", "Catalog", deviceId.c_str(), ulSN);
	if (!pElement)
	{
		return false;
	}

	int listsize = 1;
	char num[8] = {0};
	snprintf(num, 7, "%d", listsize);

	addNode(pElement, "Status", "OK");
	addNode(pElement, "SumNum", num);
	XMLElement* deviceList = m_XmlDocument.NewElement("DeviceList");
	deviceList->SetAttribute("Num", num);
	pElement->InsertEndChild(deviceList);

	XMLElement* Item = m_XmlDocument.NewElement("Item");
	addNode(Item, "DeviceID", channelId.c_str());
	addNode(Item, "Name", "dji");
	addNode(Item, "ParentID", deviceId.c_str());
	deviceList->InsertEndChild(Item);
	
	return pElement;
}

bool CXmlHelper::analyzeHeader(MessageHeader& stMessageHeader)
{
	XMLElement *pRootNode = NULL;
	XMLElement *pHeadNode = NULL;
	if (m_XmlDocument.FirstChild() == NULL || m_XmlDocument.FirstChild()->ToDeclaration() == NULL)
	{
		return false;
	}
	
	pRootNode = m_XmlDocument.FirstChildElement();
	if (pRootNode == NULL || pRootNode->Value() == NULL)
	{
		return false;
	}
	stMessageHeader.strMethod = pRootNode->Value();

	pHeadNode = pRootNode->FirstChildElement("SN");
	if (pHeadNode == NULL || (pHeadNode->GetText() == NULL))
	{
		return false;
	}
	stMessageHeader.ulSN = atol(pHeadNode->GetText());

	pHeadNode = pRootNode->FirstChildElement("DeviceID");
	if (pHeadNode == NULL || (pHeadNode->GetText() == NULL))
	{
		return false;
	}
	stMessageHeader.strDeviceID = pHeadNode->GetText();

	pHeadNode = pRootNode->FirstChildElement("CmdType");
	if (pHeadNode == NULL || (pHeadNode->GetText() == NULL))
	{
		return false;
	}
	stMessageHeader.strCmdType = pHeadNode->GetText();

	if (stMessageHeader.strCmdType == "DeviceControl")
	{
		if ((pHeadNode = pRootNode->FirstChildElement("GuardCmd")) != NULL){
			stMessageHeader.strCmdType = "GuardCmd";
		}
		else if ((pHeadNode = pRootNode->FirstChildElement("PTZCmd")) != NULL){
			stMessageHeader.strCmdType = "PTZCmd";
		}
		else if ((pHeadNode = pRootNode->FirstChildElement("TeleBoot")) != NULL){
			stMessageHeader.strCmdType = "TeleBoot";
		}
		else if ((pHeadNode = pRootNode->FirstChildElement("RecordCmd")) != NULL){
			stMessageHeader.strCmdType = "RecordCmd";
		}
		else if ((pHeadNode = pRootNode->FirstChildElement("AlarmCmd")) != NULL){
			stMessageHeader.strCmdType = "AlarmCmd";
		}
		else if ((pHeadNode = pRootNode->FirstChildElement("CatalogCmd")) != NULL){
			stMessageHeader.strCmdType = "CatalogCmd";
		}
		else{
			return false;
		}
	}
	
	return true;
}

bool CXmlHelper::analyzeRspHeader(MessageRspHeader& stMessageRspHeader)
{
	XMLElement *pRootNode = NULL;
	XMLElement *pHeadNode = NULL;
	if (m_XmlDocument.FirstChild() == NULL || m_XmlDocument.FirstChild()->ToDeclaration() == NULL)
	{
		return false;
	}
	
	pRootNode = m_XmlDocument.FirstChildElement();
	if (pRootNode == NULL || pRootNode->Value() == NULL)
	{
		return false;
	}
	stMessageRspHeader.strCategory = pRootNode->Value();

	pHeadNode = pRootNode->FirstChildElement("SN");
	if (pHeadNode == NULL || (pHeadNode->GetText() == NULL))
	{
		return false;
	}
	stMessageRspHeader.ulSN = atol(pHeadNode->GetText());

	pHeadNode = pRootNode->FirstChildElement("DeviceID");
	if (pHeadNode == NULL || (pHeadNode->GetText() == NULL))
	{
		return false;
	}
	stMessageRspHeader.strDeviceID = pHeadNode->GetText();

	pHeadNode = pRootNode->FirstChildElement("CmdType");
	if (pHeadNode == NULL || (pHeadNode->GetText() == NULL))
	{
		return false;
	}
	stMessageRspHeader.strCmdType = pHeadNode->GetText();

	pHeadNode = pRootNode->FirstChildElement("Result");
	if (pHeadNode == NULL || (pHeadNode->GetText() == NULL))
	{
		return false;
	}
	stMessageRspHeader.strResult = pHeadNode->GetText();

	return true;
}


XMLElement* CXmlHelper::addNode(XMLNode* pXmlNode, const char* pName, const char* pValue)
{
	if (!pXmlNode){
		return NULL;
	}

	XMLElement* pElement = m_XmlDocument.NewElement(pName);

	if (pElement && pValue && *pValue != '\0'){
		XMLText *pVal = m_XmlDocument.NewText(pValue);
		pElement->InsertFirstChild(pVal);
	}
	pXmlNode->InsertEndChild(pElement);
	return pElement;
}

XMLElement* CXmlHelper::addNode(XMLNode* pXmlNode, const char* pName, int iValue)
{
	if (!pXmlNode){
		return NULL;
	}

	XMLElement* pElement = m_XmlDocument.NewElement(pName);

	if (pElement){
		char str[16] = { 0 };
		snprintf(str,sizeof(str),"%d",iValue);
		XMLText *pVal = m_XmlDocument.NewText((const char*)str);
		pElement->InsertFirstChild(pVal);
	}
	pXmlNode->InsertEndChild(pElement);
	return pElement;
}

XMLElement* CXmlHelper::addNode(XMLNode* pXmlNode, const char* pName, uint64_t ulValue)
{
	if (!pXmlNode){
		return NULL;
	}

	XMLElement* pElement = m_XmlDocument.NewElement(pName);

	if (pElement){
		char str[16] = { 0 };
		snprintf(str,sizeof(str),"%lu",ulValue);
		XMLText *pVal = m_XmlDocument.NewText((const char*)str);
		pElement->InsertFirstChild(pVal);
	}
	pXmlNode->InsertEndChild(pElement);
	return pElement;
}

XMLElement* CXmlHelper::lookupMessageValue(XMLElement* pElement, char const* pKey, std::string& strValue, const char* pDefaultValue)
{
	if (pDefaultValue == NULL) {
		strValue = "";
	} else {
		strValue = pDefaultValue;
	}
	
	XMLElement* pEle = NULL;
	if (pElement == NULL)
		return NULL;
	pEle = pElement->FirstChildElement(pKey);
	if (pEle && (NULL != pEle->GetText()))
	{
		strValue = pEle->GetText();
		return pEle;
	}
	return NULL;
}

XMLElement* CXmlHelper::lookupMessageValue(XMLElement* pElement, char const* pKey, int& iValue, int iDefaultValue)
{
	iValue = iDefaultValue;
	XMLElement* pEle = NULL;
	if (pElement == NULL)
		return NULL;
	pEle = pElement->FirstChildElement(pKey);
	if (pEle && (NULL != pEle->GetText()))
	{
		iValue = atoi(pEle->GetText());
		return pEle;
	}
	return NULL;
}

XMLElement* CXmlHelper::lookupMessageValue(XMLElement* pElement, char const* pKey, unsigned int& uiValue, unsigned int uiDefaultValue)
{
	uiValue = uiDefaultValue;
	XMLElement* pEle = NULL;
	if (pElement == NULL)
		return NULL;
	pEle = pElement->FirstChildElement(pKey);
	if (pEle && (NULL != pEle->GetText()))
	{
		uiValue = atoi(pEle->GetText());
		return pEle;
	}
	return NULL;
}

XMLElement* CXmlHelper::lookupMessageValue(XMLElement* pElement, char const* pKey, int64_t& ulValue, int64_t ulDefaultValue)
{
	ulValue = ulDefaultValue;
	XMLElement* pEle = NULL;
	if (pElement == NULL)
		return NULL;
	pEle = pElement->FirstChildElement(pKey);
	if (pEle && (NULL != pEle->GetText()))
	{
		ulValue = atoll(pEle->GetText());
		return pEle;
	}
	return NULL;
}

XMLElement* CXmlHelper::lookupMessageValue(XMLElement* pElement, char const* pKey)
{
	XMLElement* pEle = NULL;
	if (pElement == NULL)
		return NULL;
	pEle = pElement->FirstChildElement(pKey);
	if (pEle)
	{
		return pEle;
	}
	return NULL;
}

XMLElement* CXmlHelper::lookupMessageValue(XMLElement* pElement, char const* pKey, double& dValue, double dDefaultValue)
{
	dValue = dDefaultValue;
	XMLElement* pEle = NULL;
	if (pElement == NULL)
		return NULL;
	pEle = pElement->FirstChildElement(pKey);
	if (pEle && (NULL != pEle->GetText()))
	{
		dValue = atof(pEle->GetText());
		return pEle;
	}
	return NULL;
}

}