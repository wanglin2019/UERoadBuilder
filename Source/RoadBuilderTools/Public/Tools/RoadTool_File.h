// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/RoadInteractiveTool.h"

#include "RoadTool_File.generated.h"

/** Builder for URoadTool_File. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_FileBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Panel-only tool: selecting it shows the export settings, which is everything the legacy FRoadTool_File
 * did - it had no interaction logic at all.
 *
 * A tool that registers a property source and no input behaviour is a legitimate ITF tool, and it is the
 * natural mapping for the legacy "this mode is showing settings X" tools: the panel follows the tool,
 * because the tool is what registered it. In the legacy layer that relationship was inverted - the mode
 * looked the settings object up from a switch on the tool identity in SetCurrentToolByType().
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_File : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
};
