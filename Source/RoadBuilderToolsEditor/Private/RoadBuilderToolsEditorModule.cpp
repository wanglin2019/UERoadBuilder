// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadBuilderToolsEditorModule.h"

#include "RoadToolsModeCommands.h"

void FRoadBuilderToolsEditorModule::StartupModule()
{
	// The mode itself needs no registration call: UAssetEditorSubsystem::RegisterEditorModes() scans for
	// UEdMode subclasses at PostEngineInit, which is why this module loads at Default.
	FRoadToolsModeCommands::Register();
}

void FRoadBuilderToolsEditorModule::ShutdownModule()
{
	FRoadToolsModeCommands::Unregister();
}

IMPLEMENT_MODULE(FRoadBuilderToolsEditorModule, RoadBuilderToolsEditor)
