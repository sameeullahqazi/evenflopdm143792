//========================================================================================
//  
//  $File: //ai_stream/rel_21_0/devtech/sdk/public/samplecode/SnippetRunner/Source/SnippetRunnerPlugin.cpp $
//
//  $Revision: #1 $
//
//  Copyright 1987 Adobe Systems Incorporated. All rights reserved.
//  
//  NOTICE:  Adobe permits you to use, modify, and distribute this file in accordance 
//  with the terms of the Adobe license agreement accompanying it.  If you have received
//  this file from a source other than Adobe, then your use, modification, or 
//  distribution of it requires the prior written permission of Adobe.
//  
//========================================================================================

#include "IllustratorSDK.h"
#include "AICSXS.h"
#include "SDKDef.h"
#include "SDKAboutPluginsHelper.h"
#include "SnippetRunnerID.h"
#include "SnippetRunnerPlugin.h"
#include "SnippetRunnerSuites.h"
#include "SnippetRunnerPanelController.h"
#include "SnippetRunnerLog.h"
#include "SnippetRunnerParameter.h"
#include "SnippetRunnerPreferences.h"
#include "SDKErrors.h"
#include "SnippetRunnerUnitTestManager.h"


// Framework includes:
#include "SnpRunnable.h"
#include "SnpSelectionHelper.h"
#include "SnpArtSetHelper.h"
#include "SnpArtHelper.h"

#include "tinyxml2.h"
#include "PdmReaderWriter.h"

using namespace ATE;
using namespace PdmMetaDataTools;

extern ImportSuite gPostStartupSuites[];

SnippetRunnerPlugin*	gPlugin = NULL;
std::map<string, map<string, AIArtHandle>>	textFrames;
map < string, map<string, PDMVariable>> variablesByHeaders;
map <string, PDMVariable> variablesByNewInfo;

tinyxml2::XMLDocument* doc;
std::map<string, tinyxml2::XMLElement*> xmlTextVariables;
/*
*/
Plugin* AllocatePlugin(SPPluginRef pluginRef)
{
	return new SnippetRunnerPlugin(pluginRef);
}

/*
*/
void FixupReload(Plugin* plugin)
{
	SnippetRunnerPlugin::FixupVTable((SnippetRunnerPlugin*)plugin);
}

/*
*/
ASErr SnippetRunnerPlugin::SetGlobal(Plugin *plugin)
{
	gPlugin = (SnippetRunnerPlugin *)plugin;
	return kNoErr;
}

/*
*/
SnippetRunnerPlugin::SnippetRunnerPlugin(SPPluginRef pluginRef) :
	Plugin(pluginRef),
	fSnippetRunnerPanelController(NULL),
	fCSXSPlugPlugSetupCompleteNotifier(NULL)
{
	strncpy(fPluginName, kSnippetRunnerPluginName, kMaxStringLength);
	fAboutPluginMenu = NULL;
	fShowHidePanelMenu = NULL;
}

/*
*/
SnippetRunnerPlugin::~SnippetRunnerPlugin()
{
}

/*
*/
ASErr SnippetRunnerPlugin::Message(char *caller, char *selector, void *message)
{
	ASErr result = kNoErr;
	try {
		result = Plugin::Message(caller, selector, message);
	}
	catch (ai::Error& ex) {
		result = ex;
	}
	catch (...) {
		result = kCantHappenErr;
	}
	if (result) {
		if (result == kUnhandledMsgErr) {
			// Defined by Plugin.hpp and used in Plugin::Message - ignore
			result = kNoErr;
		}
		else {
			// TODO should really call gPlugin->ReportError
			aisdk::report_error(result);
		}
	}
	return result;
}

/*
*/
ASErr SnippetRunnerPlugin::GoMenuItem(AIMenuMessage *message)
{
	ASErr result = kNoErr;
	try {
		if (message->menuItem == this->fAboutPluginMenu)
		{
			SDKAboutPluginsHelper aboutPluginsHelper;
			aboutPluginsHelper.PopAboutBox(message, "About SnippetRunner", kSDKDefAboutSDKCompanyPluginsAlertString);
		}
		else if (message->menuItem == this->fShowHidePanelMenu)
		{
			if (this->fSnippetRunnerPanelController)
			{
				AIBoolean visible = false;

				fSnippetRunnerPanelController->IsPrimaryStageVisible(visible);

				if (visible)
				{
					fSnippetRunnerPanelController->UnloadExtension();
				}
				else
				{
					fSnippetRunnerPanelController->LoadExtension();
				}
			}
		}
	}
	catch (ai::Error& ex) {
		result = ex;
	}
	return result;
}

/*
*/
ASErr SnippetRunnerPlugin::UpdateMenuItem(AIMenuMessage *message)
{
	// Update the Show/Hide panel menu.
	AIBoolean visible = false;

	if (fSnippetRunnerPanelController != NULL)
		fSnippetRunnerPanelController->IsPrimaryStageVisible(visible);

	// sAIMenu->CheckItem(fShowHidePanelMenu, visible);

	return kNoErr;
}

/*
*/
ASErr SnippetRunnerPlugin::StartupPlugin(SPInterfaceMessage *message)
{
	ASErr error = kNoErr;
	try {
		error = Plugin::StartupPlugin(message);
		aisdk::check_ai_error(error);
		error = this->AddNotifiers(message);
		aisdk::check_ai_error(error);
		// error = this->AddMenus(message);
		// aisdk::check_ai_error(error);
	}
	catch (ai::Error& ex) {
		error = ex;
	}
	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::ShutdownPlugin(SPInterfaceMessage *message)
{
	ASErr error = kNoErr;

	if (fSnippetRunnerPanelController)
	{
		fSnippetRunnerPanelController->RemoveEventListeners();
		delete fSnippetRunnerPanelController;
		fSnippetRunnerPanelController = NULL;
		Plugin::LockPlugin(false);
	}

	this->ReleasePostStartupSuites();
	error = Plugin::ShutdownPlugin(message);
	return error;
}

ASErr SnippetRunnerPlugin::PostStartupPlugin()
{
	ASErr error = kNoErr;
	try {
		// Acquire suites we could not get on plug-in startup.
		error = this->AcquirePostStartupSuites();
		if (error) {
			error = kNoErr;
			// Ignore - this should not be a fatal error.
			// We want the SnippetRunner panel to open and
			// we want snippets to be checking that the
			// suite pointers they depend on are not NULL
			// before using them.
		}

		// Read preferences.
		SnippetRunnerPreferences::Instance();

		// Create flash panel
		if (fSnippetRunnerPanelController == NULL)
		{
			fSnippetRunnerPanelController = new SnippetRunnerPanelController();

			error = Plugin::LockPlugin(true);
			if (error) { return error; }
		}
	}
	catch (ai::Error ex) {
		error = ex;
	}
	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::AddNotifiers(SPInterfaceMessage *message)
{
	ASErr error = kNoErr;
	try {
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner " kAIApplicationShutdownNotifier, kAIApplicationShutdownNotifier, &fAppShutdownNotifier);
		aisdk::check_ai_error(error);
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner " kAIArtSelectionChangedNotifier, kAIArtSelectionChangedNotifier, &fArtSelectionChangedNotifier);
		aisdk::check_ai_error(error);
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner " kAICSXSPlugPlugSetupCompleteNotifier, kAICSXSPlugPlugSetupCompleteNotifier, &fCSXSPlugPlugSetupCompleteNotifier);
		aisdk::check_ai_error(error);

		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner",
			kAIDocumentOpenedNotifier, NULL);
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner",
			kAIDocumentSavedNotifier, NULL);
		// error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner", kAIDocumentNewNotifier, NULL);
	}
	catch (ai::Error& ex) {
		error = ex;
	}
	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::Notify(AINotifierMessage *message)
{
	ASErr error = kNoErr;

	if (message->notifier == fAppShutdownNotifier)
	{
		SnippetRunnerLog::DeleteInstance();
		SnippetRunnerParameter::DeleteInstance();
		SnippetRunnerPreferences::Instance()->DeleteInstance();
		SnippetRunnerUnitTestManager::Instance()->DeleteInstance();
	}
	else if (message->notifier == fArtSelectionChangedNotifier)
	{
		fSnippetRunnerPanelController->HandleModelChanged();
	}
	else if (message->notifier == fCSXSPlugPlugSetupCompleteNotifier)
	{
		fSnippetRunnerPanelController->RegisterCSXSEventListeners();
	}

	ai::FilePath filePath;
	sAIDocument->GetDocumentFileSpecification(filePath);

	if (strcmp(message->type, kAIDocumentOpenedNotifier) == 0)
	{

		/*
		1.	DEFINE ALL ROW AND COLUMN HEADER NAMES BASED UPON VARIABLE NAMES

		headerInfoByVariables = {
		'varname1': ['Brand', 'Signature:']
		'varname2': ['Brand'], 'Approved:'],
		}
		2.	WHEN ITERATING THROUGH THE XML
		i.	CREATE A NEW ARRAY BY FLIPPLING THE ABOVE ARRAY
		variableNamesByHeaders = {
		'Brand': {
		'Signature': ["varname1", "varval1"],
		'Approved': "varname2", "varval2"]
		}
		''
		}
		ii.	Create row header strings
		iii.	Create col header strings
		*/

		map<string, TitleBlockHeaderInfo> headerInfoByVariables;
		map<string, string> newInfoByVariables;

		headerInfoByVariables["xmp:MarketingAppReq"] = { "Product \rMarketing ", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:MarketingApproval"] = { "Product \rMarketing ", "Approved By:", false };
		headerInfoByVariables["xmp:MarketingAppDate"] = { "Product \rMarketing ", "Date:", false };

		headerInfoByVariables["xmp:MktgBrandAppReq"] = { "Brand \rMarketing ", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:MktgBrandApproval"] = { "Brand \rMarketing ", "Approved By:", false };
		headerInfoByVariables["xmp:MktgBrandAppDate"] = { "Brand \rMarketing ", "Date:", false };

		headerInfoByVariables["xmp:MarketingDirAppReq"] = { "Marketing\rDirector ", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:MarketingDirApproval"] = { "Marketing\rDirector ", "Approved By:", false };
		headerInfoByVariables["xmp:MarketingDirAppDate"] = { "Marketing\rDirector ", "Date:", false };

		headerInfoByVariables["xmp:EngineeringAppReq"] = { "Engineering", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:EngineeringApproval"] = { "Engineering", "Approved By:", false };
		headerInfoByVariables["xmp:EngAppDate"] = { "Engineering", "Date:", false };

		headerInfoByVariables["xmp:OperationsDirAppReq"] = { "Operations\rDirector", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:OperationsDirApproval"] = { "Operations\rDirector", "Approved By:", false };
		headerInfoByVariables["xmp:OperationsDirAppDate"] = { "Operations\rDirector", "Date:", false };

		headerInfoByVariables["xmp:QAAppReq"] = { "Product\rIntegrity", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:QAApproval"] = { "Product\rIntegrity", "Approved By:", false };
		headerInfoByVariables["xmp:QAAppDate"] = { "Product\rIntegrity", "Date:", false };

		headerInfoByVariables["xmp:QADirAppReq"] = { "Quality\rManger", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:QADirApproval"] = { "Quality\rManger", "Approved By:", false };
		headerInfoByVariables["xmp:QADirAppDate"] = { "Quality\rManger", "Date:", false };

		headerInfoByVariables["xmp:LegalAppReq"] = { "Legal", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:LegalApproval"] = { "Legal", "Approved By:", false };
		headerInfoByVariables["xmp:LegalAppDate"] = { "Legal", "Date:", false };

		headerInfoByVariables["xmp:MexicoAppReq"] = { "KCM", "Approval\rNeeded", true };
		headerInfoByVariables["xmp:MexicoApproval"] = { "KCM", "Approved By:", false };
		headerInfoByVariables["xmp:MexicoAppDate"] = { "KCM", "Date:", false };

		std::map <string, AIReal> hColHeaders;
		std::map <string, AIReal> vRowHeaders;
		std::map <string, AIRealPoint> newInfoCoordinates;

		newInfoByVariables["xmp:DrawnDate"] = "Date Created:";
		newInfoByVariables["xmp:DrawnBy"] = "Author:";
		newInfoByVariables["xmp:SpecNo"] = "Spec #";
		newInfoByVariables["xmp:LegacySpecNo"] = "Legacy Spec #";
		newInfoByVariables["xmp:Revision"] = "Revision:";
		newInfoByVariables["xmp:Description"] = "Description:";
		newInfoByVariables["xmp:ECNumber"] = "EC #:";
		newInfoByVariables["xmp:Project"] = "Project #:";
		newInfoByVariables["xmp:Project Description"] = "Project Description:";
		newInfoByVariables["xmp:ArtworkType"] = "Artwork Type:";
		newInfoByVariables["xmp:Dieline"] = "Die/Drawing:";
		newInfoByVariables["xmp:Material"] = "Substrate:";


		const char* fullFilePath = filePath.GetFullPath().as_Platform().c_str();
		// sAIUser->MessageAlert(ai::UnicodeString("A document was just opened!"));
		ASErr result = kNoErr;
		try
		{
			//	Fill these up with the required values above.
			
			/*
			static const char* xml =
				"<success>true</success>"
				"<debug>Filepath passed = C:\dev\desktopDev\lampros\evenfloPdm\aiFileSamples\Balance + _WideNeck_Eng_9oz_1pk_CTN_3049 - 423_10AUG2016_OUTLINES.ai&#x0A; No smart handler available.trying packet scanning.&#x0A; &#x0A; CreatorTool = Adobe Illustrator CS6(Windows)&#x0A; terminated successfully after reading values&#x0A; </debug>"
				"<message></message>"
				"<pdm_variable_list>"
				"<pdm_variable>"
				"<variable_name>xmp:MarketingAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MarketingApproval</variable_name>"
				"<variable_value>Samee Qazi</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MarketingAppDate</variable_name>"
				"<variable_value>17-09-2010</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MktgBrandAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MktgBrandApproval</variable_name>"
				"<variable_value>Curtis Mimes</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MktgBrandAppDate</variable_name>"
				"<variable_value>20-09-2016</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MarketingDirAppReq</variable_name>"
				"<variable_value>0</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MarketingDirApproval</variable_name>"
				"<variable_value>Troy Davis</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MarketingDirAppDate</variable_name>"
				"<variable_value>23-10-2016</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:EngineeringAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:EngineeringApproval</variable_name>"
				"<variable_value>Troy Davis</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:EngAppDate</variable_name>"
				"<variable_value>12-09-2015</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:OperationsDirAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:OperationsDirApproval</variable_name>"
				"<variable_value>Troy Davis</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:OperationsDirAppDate</variable_name>"
				"<variable_value>12-09-2015</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:QAAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:QAApproval</variable_name>"
				"<variable_value>Troy Davis</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:QAAppDate</variable_name>"
				"<variable_value>12-09-2015</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:QADirAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:QADirApproval</variable_name>"
				"<variable_value>Troy Davis</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:QADirAppDate</variable_name>"
				"<variable_value>12-09-2015</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:LegalAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:LegalApproval</variable_name>"
				"<variable_value>Troy Davis</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:LegalAppDate</variable_name>"
				"<variable_value>12-09-2015</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MexicoAppReq</variable_name>"
				"<variable_value>1</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MexicoApproval</variable_name>"
				"<variable_value>Troy Davis</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:MexicoAppDate</variable_name>"
				"<variable_value>12-09-2015</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:DrawnDate</variable_name>"
				"<variable_value>11-06-2015</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:DrawnBy</variable_name>"
				"<variable_value>Muzna Roghay</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:SpecNo</variable_name>"
				"<variable_value>123</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:SpecNo</variable_name>"
				"<variable_value>123</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:LegacySpecNo</variable_name>"
				"<variable_value>12323</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:Revision</variable_name>"
				"<variable_value>123232</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:ECNumber</variable_name>"
				"<variable_value>123123534</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:Description</variable_name>"
				"<variable_value>Some description</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:Project</variable_name>"
				"<variable_value>New project</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:Project Description</variable_name>"
				"<variable_value>New project desc. New project desc.</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:ArtworkType</variable_name>"
				"<variable_value>AT</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:Dieline</variable_name>"
				"<variable_value>Drawing Line</variable_value>"
				"</pdm_variable>"
				"<pdm_variable>"
				"<variable_name>xmp:Material</variable_name>"
				"<variable_value>Copper</variable_value>"
				"</pdm_variable>"
				"</pdm_variable_list>";
			*/
			const char* xml = MetaDataReaderWriter::preCheckIn(fullFilePath).c_str();

			/***********************************************************
			//////////////////	BEGIN EXTRACTING VARIABLE CONTENT FROM THE XML CONTENT//////////////
			***********************************************************/
			doc = new tinyxml2::XMLDocument();
			doc->Parse(xml);

			tinyxml2::XMLElement* pdm_variable = doc->FirstChildElement("pdm_variable_list")->FirstChildElement("pdm_variable");
			if (pdm_variable)
			{
				do
				{
					tinyxml2::XMLElement* variable_name = pdm_variable->FirstChildElement("variable_name");
					tinyxml2::XMLElement* variable_value = pdm_variable->FirstChildElement("variable_value");
					string var_name = variable_name->GetText();
					string var_value = variable_value->GetText();

					if (headerInfoByVariables.find(var_name) != headerInfoByVariables.end())
					{
						string rowHeader = headerInfoByVariables[var_name].row;
						string columnHeader = headerInfoByVariables[var_name].column;
						variablesByHeaders[rowHeader][columnHeader] = { var_name, var_value };
						vRowHeaders[rowHeader] = 0.0;
						hColHeaders[columnHeader] = 0.0;

						xmlTextVariables[var_name] = variable_value;
					}

					if (newInfoByVariables.find(var_name) != newInfoByVariables.end())
					{
						string tmpHeader = newInfoByVariables[var_name];
						variablesByNewInfo[tmpHeader] = { var_name, var_value };
						xmlTextVariables[var_name] = variable_value;
						newInfoCoordinates[tmpHeader] = { 0.0, 0.0 };
					}

					pdm_variable = pdm_variable->NextSiblingElement();

				} while (pdm_variable);
			}
			//////////////////	END EXTRACTING VARIABLE CONTENT FROM THE XML CONTENT//////////////

			//
			AIRealMatrix headerMatrix;

			AIArtHandle artGroup = NULL;
			result = sAIArt->GetFirstArtOfLayer(NULL, &artGroup);

			SDK_ASSERT(sAITextFrame);
			SnippetRunnerLog* log = SnippetRunnerLog::Instance();
			AIArtSpec specs[1] = { { kTextFrameArt,0,0 } };
			SnpArtSetHelper textFrameArtSet(specs, 1);
			if (textFrameArtSet.GetCount() > 0)
			{
				/***********************************************************
				//////////////////	BEGIN ITERATING THE ENTIRE TEXT OF THE .AI FILE//////////////
				//	1.	GETTING COORDINATES FOR HEADER ROWS AND COLUMNS
				//	2.	GETTING COORDINATES FOR ALL OTHER REQUIRED FIELDS
				***********************************************************/
				for (size_t i = 0; i < textFrameArtSet.GetCount(); i++)
				{
					AIArtHandle textFrameArt = textFrameArtSet[i];
					TextFrameRef textFrameRef = NULL;
					result = sAITextFrame->GetATETextFrame(textFrameArt, &textFrameRef);
					aisdk::check_ai_error(result);
					ITextFrame textFrame(textFrameRef);

					SnippetRunnerLog::Indent indent;
					ITextRange textRange = textFrame.GetTextRange();

					ai::AutoBuffer<ASUnicode> contents(textRange.GetSize());
					textRange.GetContents(contents, textRange.GetSize());
					// sAIUser->MessageAlert(ai::UnicodeString(contents));
					ASInt32 strLength = textRange.GetSize();
					if (strLength > 0)
					{
						std::vector<char> vc(strLength);
						ASInt32 conLength = textRange.GetContents(&vc[0], strLength);
						if (conLength == strLength)
						{
							std::string contents;
							contents.assign(vc.begin(), vc.begin() + strLength);
							int tmp = 0;



							bool bColumnIndexPresent = hColHeaders.find(contents) != hColHeaders.end();
							bool bRowIndexPresent = vRowHeaders.find(contents) != vRowHeaders.end();
							if (bRowIndexPresent || bColumnIndexPresent)
							{

								AIArtHandle frameArt = NULL;
								error = sAITextFrame->GetAITextFrame(textFrameRef, &frameArt);

								AIRealPoint anchor;
								result = sAITextFrame->GetPointTextAnchor(textFrameArt, &anchor);
								aisdk::check_ai_error(result);

								if (bColumnIndexPresent)
								{
									hColHeaders[contents] = anchor.h;
									headerMatrix = textFrame.GetMatrix();
								}

								if (bRowIndexPresent)
								{
									vRowHeaders[contents] = anchor.v;
								}
							}

							bool bNewInfoIndexPresent = variablesByNewInfo.find(contents) != variablesByNewInfo.end();
							if (bNewInfoIndexPresent)
							{
								AIArtHandle frameArt = NULL;
								error = sAITextFrame->GetAITextFrame(textFrameRef, &frameArt);

								AIRealPoint anchor;
								result = sAITextFrame->GetPointTextAnchor(textFrameArt, &anchor);
								aisdk::check_ai_error(result);
								int len = contents.length();
								float widthFactor = 4.3;
								anchor.h += len * widthFactor;
								newInfoCoordinates[contents] = anchor;
							}
						}
					}

				}

				//////////////////	END ITERATING THE ENTIRE TEXT OF THE .AI FILE//////////////



				/**********************************************************************
				/////////////	BEGIN CREATING TEXT FIELDS FOR TABLE ///////////////////////
				************************************************************************/
				map<string, AIReal>::iterator row;

				for (row = vRowHeaders.begin(); row != vRowHeaders.end(); row++)
				{
					std::string rowIndex = row->first;
					AIReal rowTop = row->second; // rowRectTop.top;
					map<string, AIReal>::iterator col;

					map<string, AIArtHandle> innerTextRange;
					for (col = hColHeaders.begin(); col != hColHeaders.end(); col++)
					{
						std::string colIndex = col->first;
						AIReal colLeft = col->second;


						AIArtHandle newFrame;
						SnpArtHelper artHelper;

						// Get the group art that contains all the art in the current layer.
						AIArtHandle artGroup = NULL;
						result = sAIArt->GetFirstArtOfLayer(NULL, &artGroup);
						aisdk::check_ai_error(result);

						// Add the new point text item to the layer.
						AITextOrientation orient = kHorizontalTextOrientation;
						AIRealPoint newAnchor = {};
						newAnchor.h = colLeft;
						newAnchor.v = rowTop;
						AIArtHandle textFrame = NULL;
						result = sAITextFrame->NewPointText(kPlaceAboveAll, artGroup, orient, newAnchor, &textFrame);
						aisdk::check_ai_error(result);


						//The matrix for the position and rotation of the point text object. 
						//The AIRealMathSuite and the AITransformArtSuite are used. 


						// AIRealMatrix matrix;
						// matrix.Init();
						// AIReal RotationAngle = kAIRealPi2; // It works only for 360 or 0

						//Set the rotation matrix. 
						// sAIRealMath->AIRealMatrixConcatRotate(&matrix, RotationAngle);
						//Then concat a translation to the matrix. 
						// sAIRealMath->AIRealMatrixConcatTranslate(&matrix, newAnchor.h, newAnchor.v);
						//Apply the matrix to the point text object. 
						// error = sAITransformArt->TransformArt(textFrame, &matrix, 0, kTransformObjects);


						// Set the contents of the text range.
						TextRangeRef range = NULL;
						result = sAITextFrame->GetATETextRange(textFrame, &range);
						aisdk::check_ai_error(result);
						ITextRange crange(range);
						string text = "";
						if (variablesByHeaders.find(rowIndex) != variablesByHeaders.end())
						{
							if (variablesByHeaders[rowIndex].find(colIndex) != variablesByHeaders[rowIndex].end())
							{
								text = variablesByHeaders[rowIndex][colIndex].value;
								string varName = variablesByHeaders[rowIndex][colIndex].name;

								if (headerInfoByVariables[varName].bCheckbox)
								{
									text = text == "1" ? "X" : "";
								}
							}
						}
						crange.InsertAfter(ai::UnicodeString(text).as_ASUnicode().c_str());
						innerTextRange[colIndex] = textFrame;
					}
					textFrames[rowIndex] = innerTextRange;
				}
				/**********************************************************************
				/////////////	END CREATING TEXT FIELDS FOR TABLE ///////////////////////
				************************************************************************/





				/***********************************************************************
				///////////////////// BEGIN CREATING NEW FIELDS
				***********************************************************************/
				map<string, AIRealPoint>::iterator itNew;
				for (itNew = newInfoCoordinates.begin(); itNew != newInfoCoordinates.end(); itNew++)
				{
					std::string newInfoHeader = itNew->first;
					AIRealPoint newAnchor = itNew->second;


					AIArtHandle newFrame;
					SnpArtHelper artHelper;

					// Get the group art that contains all the art in the current layer.
					AIArtHandle artGroup = NULL;
					result = sAIArt->GetFirstArtOfLayer(NULL, &artGroup);
					aisdk::check_ai_error(result);

					// Add the new point text item to the layer.
					AITextOrientation orient = kHorizontalTextOrientation;

					AIArtHandle textFrame = NULL;
					result = sAITextFrame->NewPointText(kPlaceAboveAll, artGroup, orient, newAnchor, &textFrame);
					aisdk::check_ai_error(result);


					// Set the contents of the text range.
					TextRangeRef range = NULL;
					result = sAITextFrame->GetATETextRange(textFrame, &range);
					aisdk::check_ai_error(result);
					ITextRange crange(range);
					string text = "";

					if (variablesByNewInfo.find(newInfoHeader) != variablesByNewInfo.end())
					{
						text = variablesByNewInfo[newInfoHeader].value;
					}

					crange.InsertAfter(ai::UnicodeString(text).as_ASUnicode().c_str());
				}
				/***********************************************************************
				///////////////////// END CREATING NEW FIELDS
				***********************************************************************/


			}
			else
			{
				log->Write("Create some text art.");
				aisdk::check_ai_error(kBadParameterErr);
			}
		}
		catch (ai::Error& ex)
		{
			result = ex;
		}
		catch (ATE::Exception& ex)
		{
			result = ex.error;
		}
		return result;


	}
	else if (strcmp(message->type, kAIDocumentSavedNotifier) == 0)
	{
		// To Curtis Mimes: Here's where the Save event is triggered.
		ASErr result = kNoErr;
		const char* fullFilePath = filePath.GetFullPath().as_Platform().c_str();
		tinyxml2::XMLPrinter streamer;
		doc->Print(&streamer);
		// sAIUser->MessageAlert(ai::UnicodeString(streamer.CStr()));

		map<string, map<string, AIArtHandle>>::iterator outerTextRangeIterator;
		for (outerTextRangeIterator = textFrames.begin(); outerTextRangeIterator != textFrames.end(); outerTextRangeIterator++)
		{
			std::string rowIndex = outerTextRangeIterator->first;
			map<string, AIArtHandle> outerTextRange = outerTextRangeIterator->second; // rowRectTop.top;
			map<string, AIArtHandle>::iterator innerTextRangeIterator;
			for (innerTextRangeIterator = outerTextRange.begin(); innerTextRangeIterator != outerTextRange.end(); innerTextRangeIterator++)
			{
				std::string colIndex = innerTextRangeIterator->first;
				AIArtHandle textFrame = innerTextRangeIterator->second;

				TextRangeRef range = NULL;
				result = sAITextFrame->GetATETextRange(textFrame, &range);
				aisdk::check_ai_error(result);
				ITextRange textRange(range);


				ai::AutoBuffer<ASUnicode> contents(textRange.GetSize());
				textRange.GetContents(contents, textRange.GetSize());
				// sAIUser->MessageAlert(ai::UnicodeString(contents));
				ASInt32 strLength = textRange.GetSize();
				if (strLength > 0)
				{
					std::vector<char> vc(strLength);
					ASInt32 conLength = textRange.GetContents(&vc[0], strLength);
					if (conLength == strLength)
					{
						std::string contents;
						contents.assign(vc.begin(), vc.begin() + strLength);
						// To Curtis Mimes: And Here's where the modified text field content gets displayed (the contents variable).

						string prevVarValue = variablesByHeaders[rowIndex][colIndex].value;
						if (contents != prevVarValue)
						{
							string variableName = variablesByHeaders[rowIndex][colIndex].name;
							// sAIUser->MessageAlert(ai::UnicodeString(contents));
							xmlTextVariables[variableName]->SetText(contents.c_str());
						}
					}
				}

			}
		}

		tinyxml2::XMLPrinter streamer2;
		doc->Print(&streamer2);
		// sAIUser->MessageAlert(ai::UnicodeString(streamer2.CStr()));
		MetaDataReaderWriter::sync(fullFilePath, streamer2.CStr());
	}
	else if (strcmp(message->type, kAIDocumentNewNotifier) == 0)
	{
		int documentNew = 1;

	}

	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::AddMenus(SPInterfaceMessage *message)
{
	ASErr error = kNoErr;

	// Add About Plugins menu item for this plug-in.
	SDKAboutPluginsHelper aboutPluginsHelper;
	error = aboutPluginsHelper.AddAboutPluginsMenuItem(message,
		kSDKDefAboutSDKCompanyPluginsGroupName,
		ai::UnicodeString(kSDKDefAboutSDKCompanyPluginsGroupNameString),
		kSnippetRunnerPluginName "...",
		&fAboutPluginMenu);
	aisdk::check_ai_error(error);

	// Add an SDK menu group to the Windows menu.
	const char* kSDKWindowsMenuGroup = "SDKWindowGroup";
	bool exists = false;
	error = this->MenuGroupExists(kSDKWindowsMenuGroup, exists);
	aisdk::check_ai_error(error);
	if (!exists) {
		AIPlatformAddMenuItemDataUS menuItemData;
		menuItemData.groupName = kOtherPalettesMenuGroup;
		menuItemData.itemText = ai::UnicodeString("SDK");
		AIMenuItemHandle menuItemHandle = nil;
		// error = sAIMenu->AddMenuItem(message->d.self, NULL, &menuItemData, kMenuItemNoOptions, &menuItemHandle);
		// aisdk::check_ai_error(error);
		AIMenuGroup menuGroup = nil;
		// error = sAIMenu->AddMenuGroupAsSubMenu(kSDKWindowsMenuGroup, kMenuGroupSortedAlphabeticallyOption, menuItemHandle, &menuGroup);
		// aisdk::check_ai_error(error);
	}

	// Add menu item for this plug-in under the company's about plug-ins menu group.
	AIPlatformAddMenuItemDataUS showHidePanelMenuData;
	showHidePanelMenuData.groupName = kSDKWindowsMenuGroup;
	showHidePanelMenuData.itemText = ai::UnicodeString(kSnippetRunnerPluginName);
	AIMenuItemHandle showHidePanelMenuItemHandle = nil;
	// error = sAIMenu->AddMenuItem(message->d.self, NULL, &showHidePanelMenuData, kMenuItemWantsUpdateOption, &showHidePanelMenuItemHandle);
	// aisdk::check_ai_error(error);
	fShowHidePanelMenu = showHidePanelMenuItemHandle;

	return error;
}

/*
*/
AIErr SnippetRunnerPlugin::MenuGroupExists(const char* targetGroupName, bool& groupAlreadyMade)
{
	AIErr error = kNoErr;
	groupAlreadyMade = false;
	ai::int32 count = 0;
	AIMenuGroup dummyGroup = nil;
	error = sAIMenu->CountMenuGroups(&count);
	if (error) return error;
	for (ai::int32 i = 0; i < count; i++)
	{
		error = sAIMenu->GetNthMenuGroup(i, &dummyGroup);
		aisdk::check_ai_error(error);
		const char* name;
		error = sAIMenu->GetMenuGroupName(dummyGroup, &name);
		aisdk::check_ai_error(error);
		if (std::strcmp(name, targetGroupName) == 0)
		{
			groupAlreadyMade = true;
			break;
		}
	}
	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::AcquirePostStartupSuites()
{
	ASErr error = kNoErr;
	for (int i = 0; gPostStartupSuites[i].name != nil; ++i) {
		if (gPostStartupSuites[i].suite != nil) {
			ASErr tmperr = sSPBasic->AcquireSuite(gPostStartupSuites[i].name,
				gPostStartupSuites[i].version,
				(const void **)gPostStartupSuites[i].suite);
			SDK_ASSERT_MSG_NOTHROW(!tmperr,
				aisdk::format_args("AcquireSuite failed for suite=%s version=%d",
					gPostStartupSuites[i].name,
					gPostStartupSuites[i].version));
			if (tmperr && !error) {
				// A suite could not be acquired - note first error encountered for later return to caller then carry on.
				error = tmperr;
			}
		}
	}
	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::ReleasePostStartupSuites()
{
	ASErr error = kNoErr;

	for (int i = 0; gPostStartupSuites[i].name != nil; ++i) {
		if (gPostStartupSuites[i].suite != nil) {
			void **s = (void **)gPostStartupSuites[i].suite;
			if (*s != nil) {
				ASErr tmperr = sSPBasic->ReleaseSuite(gPostStartupSuites[i].name, gPostStartupSuites[i].version);
				*s = nil;
				SDK_ASSERT_MSG_NOTHROW(!tmperr,
					aisdk::format_args("ReleaseSuite failed for suite=%s version=%d",
						gPostStartupSuites[i].name,
						gPostStartupSuites[i].version));
				if (tmperr && !error) {
					// A suite could not be released - note first error encountered for later return to caller then carry on.
					error = tmperr;
				}
			}
		}
	}

	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::GoTimer(AITimerMessage* message)
{
	// This plug-in only has one timer - used for running unit tests.
	SnippetRunnerUnitTestManager::Instance()->GoTimer(message);
	return kNoErr;
}

/*
*/
void SnippetRunnerPlugin::NotifyLogChanged()
{
	if (fSnippetRunnerPanelController)
	{
		fSnippetRunnerPanelController->HandleLogChanged();
	}
}

/*
*/
void SnippetRunnerPlugin::NotifyEndUnitTest()
{
	if (fSnippetRunnerPanelController)
	{
		fSnippetRunnerPanelController->HandleModelChanged();
	}
}

// End SnippetRunnerPlugin.cpp