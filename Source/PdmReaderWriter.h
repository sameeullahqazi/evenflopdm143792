#ifndef __PDM_READER_WRITER_H_
#define __PDM_READER_WRITER_H_

#pragma once

#ifdef PDMMETADATATOOLS_EXPORTS  
#define PDMMETADATATOOLS_API __declspec(dllexport)   
#else  
#define PDMMETADATATOOLS_API __declspec(dllimport)   
#endif

#include <vector>
extern "C"
{
	namespace PdmMetaDataTools
	{
		class MetaDataReaderWriter
		{
		public:
			// does a checkin of an AI file with the char array encoded as a widestring (e.g. from VB/C#.net)
			__declspec(dllexport) static int checkinByNumberedFile(int fileNumber);

			// does a checkin of an AI file with a typical char array
			__declspec(dllexport) static std::string preCheckIn(const char * filePath);

			//syncs the XMP data on an AI file with the given variable data 
			__declspec(dllexport) static std::string sync(const char * filePath, const char * xmlVariableString);

			//syncs the XMP data on an AI file with the given variable data 
			__declspec(dllexport) static int syncByNumberedFile(int fileNumber);

			//just an alias for preCheckin, returns xml string with debug info and meta data values in xml string
			_declspec(dllexport) static std::string getXmpMetaData(const char * filePath);

			//update meta data with given variable values
			_declspec(dllexport) static std::string updateMetaData(const char * filePath, std::vector<std::string> variableNames, std::vector<std::string> variableValues);

			//dummy function for testing
			_declspec(dllexport) static int dllAdd(int a, int b);


		};
	}
}
#endif // __PDM_READER_WRITER_H_