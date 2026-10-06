// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_File.h"

#include "Settings.h"

UInteractiveTool* URoadTool_FileBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_File* NewTool = NewObject<URoadTool_File>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_File::Setup()
{
	UInteractiveTool::Setup();

	// The panel object is the shared settings CDO - the very object the legacy mode handed to its
	// inspector. AddToolPropertySource() accepts any UObject, so no adapter class is needed.
	//
	// Because USettings_File is not a UInteractiveToolPropertySet, the tool is not subscribed to its
	// changes, which is correct here: the tool does not react to them. The export action is a
	// CallInEditor UFUNCTION and the property panel renders those as buttons.
	AddToolPropertySource(GetMutableDefault<USettings_File>());
}
