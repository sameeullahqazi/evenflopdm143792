#define WIN_ENV 1

// Must be defined to instantiate template classes
#define TXMP_STRING_TYPE std::string 

// Must be defined to give access to XMPFiles
#define XMP_INCLUDE_XMPFILES 1 

#include "IllustratorSDK.h"
// Ensure XMP templates are instantiated
#include "XMP.incl_cpp"

// Provide access to the API
#include "XMP.hpp"
#include "stdafx.h"


#include <map>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <typeinfo>
#include <stdlib.h>
#include "tinyxml.h"
#include "PdmReaderWriter.h"

using namespace std;

namespace PdmMetaDataTools
{
	int MetaDataReaderWriter::checkinByNumberedFile(int tempFilePathNumber) {
		string fileNumberString = to_string(tempFilePathNumber);
		string tempEnv = string(getenv("TEMP")) + "\\";
		string tempFileName = tempEnv + to_string(tempFilePathNumber);
		ifstream file(tempFileName);
		string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

		//close and delete the file
		file.close();
		remove(tempFileName.c_str());
		string filePathStr = string(content);

		//trim newlines from file
		while (filePathStr.find("\n") != string::npos)
		{
			filePathStr.erase(filePathStr.find("\n"), 2);
		}
		while (filePathStr.find("\r\n") != string::npos)
		{
			filePathStr.erase(filePathStr.find("\r\n"), 2);
		}
		string xmlResult = MetaDataReaderWriter::preCheckIn(filePathStr.c_str());

		//wrap the elements in a root element
		xmlResult = "<root>" + xmlResult + "</root>";
		//prepare another temp file
		string tempResultFileName = tempFileName + string("preCheckinResult.txt");
		ofstream resultFile;
		resultFile.open(tempResultFileName);
		resultFile << xmlResult;
		resultFile.close();
		return tempFilePathNumber;
	}

	int MetaDataReaderWriter::dllAdd(int a, int b) {
		int c = a + b;
		return c;
	}


	TiXmlDocument getNewResultXmlDoc(bool resultSuccessFlag, string debugString, std::string resultMessage) {
		//create the xml document	
		TiXmlDocument resultXml;

		//prepare output for returning as xml string
		string resultSuccessString = resultSuccessFlag ? "true" : "false";
		TiXmlText * debugLogTxt = new TiXmlText(debugString.c_str());
		TiXmlText * resultSuccessTxt = new TiXmlText(resultSuccessString.c_str());
		TiXmlText * resultmessageTxt = new TiXmlText(resultMessage.c_str());

		//make xml elements to store info
		TiXmlElement * resultFlagEl = new TiXmlElement("success");
		TiXmlElement * debugEl = new TiXmlElement("debug");
		TiXmlElement * resultMessageEl = new TiXmlElement("message");

		//attach the values to the appropriate elements
		resultFlagEl->LinkEndChild(resultSuccessTxt);
		debugEl->LinkEndChild(debugLogTxt);
		resultMessageEl->LinkEndChild(resultmessageTxt);

		//attach the elements to the document
		resultXml.LinkEndChild(resultFlagEl);
		resultXml.LinkEndChild(debugEl);
		resultXml.LinkEndChild(resultMessageEl);

		return resultXml;
	}

	// does a checkin of an AI file with a typical char array
	string MetaDataReaderWriter::preCheckIn(const char * filePath) {


		//these variable are temporary storage for results/log/debugging
		string resultMessage;
		bool resultSuccessFlag;

		//create the element for storing all of the pdm variables
		TiXmlElement * variableListEl = new TiXmlElement("pdm_variable_list");

		//make the debug log string stream
		stringstream debugStream;

		if (!SXMPMeta::Initialize())
		{
			resultMessage = "Could not initialize toolkit!";
			resultSuccessFlag = false;


		}
		XMP_OptionBits options = 0;

		// Must initialize SXMPFiles before we use it
		if (!SXMPFiles::Initialize(options))
		{
			resultMessage = "Could not initialize SXMPFiles.";
			resultSuccessFlag = false;
		}

		try
		{
			// Options to open the file with - read only and use a file handler
			XMP_OptionBits opts = kXMPFiles_OpenForRead | kXMPFiles_OpenUseSmartHandler;

			bool ok;
			SXMPFiles myFile;
			std::string status = "";

			debugStream << "Filepath passed = " << string(filePath) << endl;

			// First we try and open the file
			ok = myFile.OpenFile(string(filePath), kXMP_UnknownFile, opts);
			if (!ok)
			{
				status = "No smart handler available. trying packet scanning.\n";

				// Now try using packet scanning
				opts = kXMPFiles_OpenForUpdate | kXMPFiles_OpenUsePacketScanning;
				ok = myFile.OpenFile(string(filePath), kXMP_UnknownFile, opts);
			}


			// If the file is open then read the metadata
			if (ok)
			{
				debugStream << status << endl;
				//out << filePath << " is opened successfully" << endl;
				// Create the xmp object and get the xmp data
				SXMPMeta meta;
				myFile.GetXMP(&meta);

				bool exists;

				// Read a simple property
				string simpleValue;  //Stores the value for the property
				exists = meta.GetProperty(kXMP_NS_XMP, "CreatorTool", &simpleValue, NULL);
				if (exists)
					debugStream << "CreatorTool = " << simpleValue << endl;
				else
					simpleValue.clear();


				std::stringstream ss;

				//loop through all of the variables and store them in xml elements
				string schemaNS;
				string propPath;
				string propVal;
				SXMPIterator iter(meta);
				while (iter.Next(&schemaNS, &propPath, &propVal)) {
					TiXmlElement * variableEl = new TiXmlElement("pdm_variable");
					TiXmlElement * variableNameEl = new TiXmlElement("variable_name");
					TiXmlElement * variableValueEl = new TiXmlElement("variable_value");

					TiXmlText * variableNameTxt = new TiXmlText(propPath.c_str());
					TiXmlText * variableValueTxt = new TiXmlText(propVal.c_str());

					variableNameEl->LinkEndChild(variableNameTxt);
					variableValueEl->LinkEndChild(variableValueTxt);
					variableEl->LinkEndChild(variableNameEl);
					variableEl->LinkEndChild(variableValueEl);

					variableListEl->LinkEndChild(variableEl);

					ss << propPath << " = " << propVal << endl;


				}
				resultSuccessFlag = true;
				//resultMessage = ss.str().c_str();
			}
			else
			{
				resultMessage = "unabled to open file and read metadata";
				resultSuccessFlag = false;
			}
			myFile.CloseFile();
		}
		catch (XMP_Error & e)
		{
			XMP_StringPtr ePtr = e.GetErrMsg();
			resultMessage = *ePtr;
			resultSuccessFlag = false;

			debugStream << "error caught: " << ePtr << endl;
		}

		// Terminate the toolkit
		SXMPFiles::Terminate();
		SXMPMeta::Terminate();
		debugStream << "terminated successfully after reading values" << endl;

		//prepare output for returning as xml string
		TiXmlDocument  resultXml = getNewResultXmlDoc(resultSuccessFlag, debugStream.str(), resultMessage);


		resultXml.LinkEndChild(variableListEl);

		TiXmlPrinter printer;
		resultXml.Accept(&printer);
		string resultXmlString = std::string(printer.CStr());
		//delete the pointers
		variableListEl->Clear();
		return resultXmlString;
	}

	//syncs the XMP data on an AI file with the given variable data 
	std::string MetaDataReaderWriter::sync(const char * filePath, const char * xmlVariableString) {

		//parallel arrays for storing name,value pairs
		std::vector<std::string> variableNames;
		std::vector<std::string> variableValues;

		//constants for xml element names
		const std::string PDM_VARIABLE_LIST = "pdm_variable_list";
		const std::string PDM_VARIABLE = "pdm_variable";
		const std::string PDM_VARIABLE_NAME = "variable_name";
		const std::string PDM_VARIABLE_VALUE = "variable_value";

		//create an xml doc from the variable string
		TiXmlDocument updatedPdmDataXmlDoc;
		updatedPdmDataXmlDoc.Parse(xmlVariableString, 0, TIXML_ENCODING_UTF8);

		//load the pdm variable list element from updated PDM card
		TiXmlElement * updatedPdmVariableListEl = updatedPdmDataXmlDoc.FirstChildElement(PDM_VARIABLE_LIST.c_str());

		//loop through the children
		TiXmlNode * child = 0;
		while (child = updatedPdmVariableListEl->IterateChildren(child)) {

			//if the current child is empty, skip it to avoid null ptr issues
			if (child->NoChildren()) {
				continue;
			}
			//store updated variable values
			const char * name = child->FirstChildElement(PDM_VARIABLE_NAME.c_str())->GetText();
			const char * value = child->FirstChildElement(PDM_VARIABLE_VALUE.c_str())->GetText();
			if (name != NULL && value != NULL) {
				variableNames.push_back(std::string(name));
				variableValues.push_back(std::string(value));
			}
		}

		//std::debugStream << printer.CStr() << endl;
		std::string result = MetaDataReaderWriter::updateMetaData(filePath, variableNames, variableValues);
		return result;
	}

	//syncs the XMP data on an AI file with the given variable data 
	int MetaDataReaderWriter::syncByNumberedFile(int fileNumber) {
		return 1;
	}

	//alias for preCheckin--just reads metadata and gives back some debugging info in an xml string
	std::string MetaDataReaderWriter::getXmpMetaData(const char * filePath) {
		return MetaDataReaderWriter::preCheckIn(filePath);
	}

	std::string MetaDataReaderWriter::updateMetaData(const char * filePath, std::vector<std::string> variableNames, std::vector<std::string> variableValues) {
		//make the debug log string stream
		stringstream debugStream;

		//indicates if the meta data was changed successfully
		bool successFlag = false;

		XMP_OptionBits options = 0;
		// Must initialize SXMPFiles before we use it
		if (SXMPFiles::Initialize(options))
		{
			try
			{
				// Options to open the file with - open for editing and use a smart handler
				XMP_OptionBits opts = kXMPFiles_OpenForUpdate | kXMPFiles_OpenUseSmartHandler;

				bool ok;
				SXMPFiles myFile;
				std::string status = "";


				// First we try and open the file
				ok = myFile.OpenFile(filePath, kXMP_UnknownFile, opts);
				if (!ok)
				{
					status += "No smart handler available for " + std::string(filePath) + "\n";
					status += "Trying packet scanning.\n";

					// Now try using packet scanning
					opts = kXMPFiles_OpenForUpdate | kXMPFiles_OpenUsePacketScanning;
					ok = myFile.OpenFile(filePath, kXMP_UnknownFile, opts);
				}

				// If the file is open then read get the XMP data
				if (ok)
				{
					debugStream << status << endl;
					debugStream << filePath << " is opened successfully" << endl;
					// Create the XMP object and get the XMP data
					SXMPMeta meta;
					myFile.GetXMP(&meta);

					for (int index = 0; index < variableNames.size(); index++) {
						meta.SetProperty(kXMP_NS_XMP, variableNames[index].c_str(), variableValues[index], 0);
					}


					// log the properties to show changes
					debugStream << "After update:" << endl;

					// Display the properties again to show changes
					debugStream << "After Appending Properties:" << endl;

					// Check we can put the XMP packet back into the file
					if (myFile.CanPutXMP(meta))
					{
						// If so then update the file with the modified XMP
						myFile.PutXMP(meta);
						successFlag = true;
					}

					// Close the SXMPFile.  This *must* be called.  The XMP is not
					// actually written and the disk file is not closed until this call is made.
					myFile.CloseFile();


				}
				else
				{
					debugStream << "Unable to open " << filePath << endl;
				}
			}
			catch (XMP_Error & e)
			{
				debugStream << "ERROR: " << e.GetErrMsg() << endl;
			}

			// Terminate the toolkit
			SXMPFiles::Terminate();
			SXMPMeta::Terminate();

		}
		else
		{
			debugStream << "Could not initialize SXMPFiles.";

		}

		//print document to string
		TiXmlDocument resultXml = getNewResultXmlDoc(successFlag, debugStream.str(), "");
		TiXmlPrinter printer;
		resultXml.Accept(&printer);

		//clear the memory from the xml doc
		resultXml.Clear();
		return std::string(printer.CStr());
	}

}
