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

using namespace ATE;

extern ImportSuite gPostStartupSuites[];

SnippetRunnerPlugin*	gPlugin = NULL;
std::map<string, map<string, AIArtHandle>>	textFrames;

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
	SnippetRunnerPlugin::FixupVTable((SnippetRunnerPlugin*) plugin);
}

/*
*/
ASErr SnippetRunnerPlugin::SetGlobal(Plugin *plugin)
{
	gPlugin = (SnippetRunnerPlugin *) plugin;
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
ASErr SnippetRunnerPlugin::GoMenuItem( AIMenuMessage *message )
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

				if(visible)
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
	catch(ai::Error& ex) {
		result = ex;
	}
	return result;
}

/*
*/
ASErr SnippetRunnerPlugin::UpdateMenuItem( AIMenuMessage *message )
{
	// Update the Show/Hide panel menu.
	AIBoolean visible = false;

	if ( fSnippetRunnerPanelController != NULL )
		fSnippetRunnerPanelController->IsPrimaryStageVisible(visible);
	
	// sAIMenu->CheckItem(fShowHidePanelMenu, visible);

	return kNoErr;
}

/*
*/
ASErr SnippetRunnerPlugin::StartupPlugin( SPInterfaceMessage *message )
{
	ASErr error = kNoErr;
	try {
		error = Plugin::StartupPlugin( message );
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
ASErr SnippetRunnerPlugin::ShutdownPlugin( SPInterfaceMessage *message )
{
	ASErr error = kNoErr;
	
	if ( fSnippetRunnerPanelController )
	{
		fSnippetRunnerPanelController->RemoveEventListeners();
		delete fSnippetRunnerPanelController;
		fSnippetRunnerPanelController = NULL;
		Plugin::LockPlugin(false);
	}
	
	this->ReleasePostStartupSuites();
	error = Plugin::ShutdownPlugin( message );
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
ASErr SnippetRunnerPlugin::AddNotifiers( SPInterfaceMessage *message )
{
	ASErr error = kNoErr;
	try {
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner " kAIApplicationShutdownNotifier, kAIApplicationShutdownNotifier, &fAppShutdownNotifier);
		aisdk::check_ai_error(error);
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner " kAIArtSelectionChangedNotifier,  kAIArtSelectionChangedNotifier, &fArtSelectionChangedNotifier);
		aisdk::check_ai_error(error);
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner " kAICSXSPlugPlugSetupCompleteNotifier, kAICSXSPlugPlugSetupCompleteNotifier, &fCSXSPlugPlugSetupCompleteNotifier);
		aisdk::check_ai_error(error);

		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner",
			kAIDocumentOpenedNotifier, NULL);
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner",
			kAIDocumentSavedNotifier, NULL);
		error = sAINotifier->AddNotifier(message->d.self, "SnippetRunner",
			kAIDocumentNewNotifier, NULL);
	}
	catch (ai::Error& ex) {
		error = ex;
	}
	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::Notify( AINotifierMessage *message )
{
	ASErr error = kNoErr;

	if ( message->notifier == fAppShutdownNotifier )
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
	else if ( message->notifier == fCSXSPlugPlugSetupCompleteNotifier )
	{
		fSnippetRunnerPanelController->RegisterCSXSEventListeners();
	}

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
		headerInfoByVariables["BrandManagerApproval"] = { "Brand", "Signature:" };
		headerInfoByVariables["BrandManagerAppDate"] = { "Brand", "Date:" };
		headerInfoByVariables["MarketingDirApproval"] = { "Marketing", "Signature:" };
		headerInfoByVariables["MarketingDirAppDate"] = { "Marketing", "Date:" };
		headerInfoByVariables["ProductEngineerApproval"] = { "Engineer ", "Signature:" };
		headerInfoByVariables["ProductEngineerAppDate"] = { "Engineer ", "Date:" };
		headerInfoByVariables["EngineeringDirApproval"] = { "Engineering ", "Signature:" };
		headerInfoByVariables["EngineeringDirAppDate"] = { "Engineering ", "Date:" };
		
		headerInfoByVariables["ProductIntegrityApproval"] = { "Integrity ", "Signature:" };
		headerInfoByVariables["ProductIntegrityAppDate"] = { "Integrity ", "Date:" };

		headerInfoByVariables["QADirApproval"] = { "Quality ", "Signature:" };
		headerInfoByVariables["QADirAppDate"] = { "Quality ", "Date:" };

		headerInfoByVariables["LegalApproval"] = { "Legal ", "Signature:" };
		headerInfoByVariables["LegalAppDate"] = { "Legal ", "Date:" };
		

		map < string, map<string, string>> variableValuesByHeaders;
		std::map <string, AIReal> hColHeaders;
		std::map <string, AIReal> vRowHeaders;
		int documentOpened = 1;
		// sAIUser->MessageAlert(ai::UnicodeString("A document was just opened!"));
		ASErr result = kNoErr;
		try
		{
			//	Fill these up with the required values above.
			static const char* xml =
				"<success>true</success>"
				"<debug>Filepath passed = C:\dev\desktopDev\lampros\evenfloPdm\aiFileSamples\Balance + _WideNeck_Eng_9oz_1pk_CTN_3049 - 423_10AUG2016_OUTLINES.ai&#x0A; No smart handler available.trying packet scanning.&#x0A; &#x0A; CreatorTool = Adobe Illustrator CS6(Windows)&#x0A; terminated successfully after reading values&#x0A; </debug>"
				"<message></message>"
				"<pdm_variable_list>"
					"<pdm_variable>"
						"<variable_name>BrandManagerApproval</variable_name>"
						"<variable_value>Brand Manager Name</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>BrandManagerAppDate</variable_name>"
						"<variable_value>Jul 12th, 2017</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>MarketingDirApproval</variable_name>"
						"<variable_value>Marteking Director Name</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>MarketingDirAppDate</variable_name>"
						"<variable_value>07/20/2017</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>ProductEngineerApproval</variable_name>"
						"<variable_value>Prod Engg Name</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>ProductEngineerAppDate</variable_name>"
						"<variable_value>06/12/2017</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>EngineeringDirApproval</variable_name>"
						"<variable_value>Eng Dir Name</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>EngineeringDirAppDate</variable_name>"
						"<variable_value>01/11/2017</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>ProductIntegrityApproval</variable_name>"
						"<variable_value>PI Name</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>ProductIntegrityAppDate</variable_name>"
						"<variable_value>04/10/2017</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>QADirApproval</variable_name>"
						"<variable_value>QA Dir Name</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>QADirAppDate</variable_name>"
						"<variable_value>06/09/2017</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>LegalApproval</variable_name>"
						"<variable_value>Legal Name</variable_value>"
					"</pdm_variable>"
					"<pdm_variable>"
						"<variable_name>LegalAppDate</variable_name>"
						"<variable_value>02/21/2017</variable_value>"
					"</pdm_variable>"
				"</pdm_variable_list>";
			tinyxml2::XMLDocument doc;
			doc.Parse(xml);
			// // doc.LoadFile("variables.xml");
			tinyxml2::XMLElement* pdm_variable = doc.FirstChildElement("pdm_variable_list")->FirstChildElement("pdm_variable");
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
						variableValuesByHeaders[rowHeader][columnHeader] = var_value;
						vRowHeaders[rowHeader] = 0.0;
						hColHeaders[columnHeader] = 0.0;
					}

					pdm_variable = pdm_variable->NextSiblingElement();

				} while (pdm_variable);
			}
			
			DocumentTextResourcesRef docTextResourcesRef = NULL;
			ASErr result = sAIDocument->GetDocumentTextResources(&docTextResourcesRef);
			aisdk::check_ai_error(result);

			IDocumentTextResources resources(docTextResourcesRef);
			ICharStyle normalCharStyle = resources.GetNormalCharStyle();
			ICharFeatures features;
			features.SetCharacterRotation(90);

			// normalCharStyle.ReplaceOrAddFeatures(features);

		
			AIRealMatrix headerMatrix;

			AIArtHandle artGroup = NULL;
			result = sAIArt->GetFirstArtOfLayer(NULL, &artGroup);

			SDK_ASSERT(sAITextFrame);
			SnippetRunnerLog* log = SnippetRunnerLog::Instance();
			AIArtSpec specs[1] = { { kTextFrameArt,0,0 } };
			SnpArtSetHelper textFrameArtSet(specs, 1);
			if (textFrameArtSet.GetCount() > 0)
			{
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
							/*
							if (contents == "Signature:" 
								|| contents == "Comments:" || contents == "Approved:" 
								|| contents == "Rejected:" || contents == "Date:" || 
								contents == "Brand" || contents == "Marketing"
								|| contents == "Engineer " || contents == "Engineering " 
								|| contents == "Integrity "
								|| contents == "PI" || contents == "Legal")
							*/
							bool bColumnIndexPresent = hColHeaders.find(contents) != hColHeaders.end();
							bool bRowIndexPresent = vRowHeaders.find(contents) != vRowHeaders.end();
							if(bRowIndexPresent || bColumnIndexPresent)
							{
								
								AIArtHandle frameArt = NULL;
								error = sAITextFrame->GetAITextFrame(textFrameRef, &frameArt);
								// aisdk::check_ai_error(result);


								AIRealPoint anchor;
								result = sAITextFrame->GetPointTextAnchor(textFrameArt, &anchor);
								aisdk::check_ai_error(result);

								//if (contents == "Signature:"
								//	|| contents == "Comments:" || contents == "Approved:"
								//	|| contents == "Rejected:" || contents == "Date:")
								if(bColumnIndexPresent)
								{
									hColHeaders[contents] = anchor.v;
									headerMatrix = textFrame.GetMatrix();
								}

								/*if (contents == "Brand" || contents == "Marketing"
									|| contents == "Engineer " 
									|| contents == "Engineering "
									|| contents == "Integrity "
									|| contents == "PI" || contents == "Legal")*/
								if(bRowIndexPresent)
								{
									vRowHeaders[contents] = anchor.h;
								}
								
								
							}
						}
					}

				}

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
						newAnchor.v = colLeft;
						newAnchor.h = rowTop;
						AIArtHandle textFrame = NULL;
						result = sAITextFrame->NewPointText(kPlaceAboveAll, artGroup, orient, { 0, 0 }, &textFrame);
						aisdk::check_ai_error(result);


						//The matrix for the position and rotation of the point text object. 
						//The AIRealMathSuite and the AITransformArtSuite are used. 
						AIRealMatrix matrix;
						matrix.Init();
						AIReal RotationAngle = kAIRealPi2; // It works only for 360 or 0

						//
						//Set the rotation matrix. 
						sAIRealMath->AIRealMatrixConcatRotate(&matrix, RotationAngle);
						//Then concat a translation to the matrix. 
						sAIRealMath->AIRealMatrixConcatTranslate(&matrix, newAnchor.h, newAnchor.v);
						// sAIRealMath->AIRealMatrixInvert(&matrix);
						//Apply the matrix to the point text object. 
						error = sAITransformArt->TransformArt(textFrame, &matrix, 0, kTransformObjects);


						// Set the contents of the text range.
						TextRangeRef range = NULL;
						result = sAITextFrame->GetATETextRange(textFrame, &range);
						aisdk::check_ai_error(result);
						ITextRange crange(range);
						string text = "";
						if (variableValuesByHeaders.find(rowIndex) != variableValuesByHeaders.end())
						{
							if (variableValuesByHeaders[rowIndex].find(colIndex) != variableValuesByHeaders[rowIndex].end())
							{
								text = variableValuesByHeaders[rowIndex][colIndex];
							}
						}
						crange.InsertAfter(ai::UnicodeString(text).as_ASUnicode().c_str());
						
						// crange.Select();
						// error = sAITransformArt->TransformArt(textFrame, &headerMatrix, 0, kTransformObjects);

						// ITextRanges tmpTextRanges = crange.GetTextSelection();
						// tmpTextRanges.SetLocalCharFeatures(features);
						// crange.SetLocalCharFeatures(features);
						innerTextRange[colIndex] = textFrame;
						

					}
					textFrames[rowIndex] = innerTextRange;
				}

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
		int documentSaved = 1;
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
						sAIUser->MessageAlert(ai::UnicodeString(contents));
					}
				}

			}
		}
	}
	else if (strcmp(message->type, kAIDocumentNewNotifier) == 0)
	{
		int documentNew = 1;
		sAIUser->MessageAlert(ai::UnicodeString("A new document was just created!"));
		
		ASErr result = kNoErr;
		try {
			// Get the group art that contains all the art in the current layer.
			AIArtHandle artGroup = NULL;
			result = sAIArt->GetFirstArtOfLayer(NULL, &artGroup);
			aisdk::check_ai_error(result);

			// Add the new point text item to the layer.
			AITextOrientation orient = kHorizontalTextOrientation;
			AIRealPoint anchor = {};
			anchor.h = -10;
			anchor.v = -50;
			AIArtHandle textFrame = NULL;
			result = sAITextFrame->NewPointText(kPlaceAboveAll, artGroup, orient, anchor, &textFrame);
			aisdk::check_ai_error(result);

			// Set the contents of the text range.
			TextRangeRef range = NULL;
			result = sAITextFrame->GetATETextRange(textFrame, &range);
			aisdk::check_ai_error(result);
			ITextRange crange(range);
			crange.InsertAfter(ai::UnicodeString("New point text item").as_ASUnicode().c_str());

			//Choose the rotation angle for the point text object. 
			//For instance 90°. 
			AIReal RotationAngle;
			RotationAngle = kAIRealPi2;
			
			//The matrix for the position and rotation of the point text object. 
			//The AIRealMathSuite and the AITransformArtSuite are used. 
			AIRealMatrix matrix;
			//Set the rotation matrix. 
			sAIRealMath->AIRealMatrixSetRotate(&matrix, RotationAngle);
			//Then concat a translation to the matrix. 
			// sAIRealMath->AIRealMatrixConcatTranslate(&matrix, anchor.h, anchor.v);
			//Apply the matrix to the point text object. 
			error = sAITransformArt->TransformArt(textFrame, &matrix, 0, kTransformObjects);
			
		}
		catch (ai::Error& ex) {
			result = ex;
		}
		catch (ATE::Exception& ex) {
			result = ex.error;
		}
		
	}

	return error;
}

/*
*/
ASErr SnippetRunnerPlugin::AddMenus( SPInterfaceMessage *message )
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
	error = sAIMenu->CountMenuGroups( &count );
	if ( error ) return error;
	for (ai::int32 i = 0; i < count; i++)
	{
		error = sAIMenu->GetNthMenuGroup( i, &dummyGroup );
		aisdk::check_ai_error(error);
		const char* name;
		error = sAIMenu->GetMenuGroupName( dummyGroup, &name );
		aisdk::check_ai_error(error);
		if ( std::strcmp(name, targetGroupName ) == 0 )
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
	for ( int i = 0; gPostStartupSuites[i].name != nil; ++i ) {
		if ( gPostStartupSuites[i].suite != nil ) {
			ASErr tmperr = sSPBasic->AcquireSuite( gPostStartupSuites[i].name, 
										gPostStartupSuites[i].version, 
										(const void **) gPostStartupSuites[i].suite );
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

	for ( int i = 0; gPostStartupSuites[i].name != nil; ++i ) {
		if ( gPostStartupSuites[i].suite != nil ) {
			void **s = (void **) gPostStartupSuites[i].suite;
			if (*s != nil) {
				ASErr tmperr = sSPBasic->ReleaseSuite( gPostStartupSuites[i].name, gPostStartupSuites[i].version );
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