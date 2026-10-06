// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_Settings.h"

#include "Settings.h"

UInteractiveTool* URoadTool_SettingsBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_Settings* NewTool = NewObject<URoadTool_Settings>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_Settings::Setup()
{
	UInteractiveTool::Setup();

	// Shared settings CDO, exactly as URoadTool_File does with USettings_File: the two tools differ only
	// in which object they show.
	AddToolPropertySource(GetMutableDefault<USettings_Global>());
}
