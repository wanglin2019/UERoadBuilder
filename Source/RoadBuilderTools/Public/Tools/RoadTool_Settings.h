// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/RoadInteractiveTool.h"

#include "RoadTool_Settings.generated.h"

/** Builder for URoadTool_Settings. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_SettingsBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Panel-only tool showing the global settings, the counterpart of the legacy FRoadTool_Settings.
 *
 * Kept as its own class rather than sharing one "settings tool" with URoadTool_File: they are two
 * distinct entries in the palette with two distinct settings objects, and the settings object is the
 * entire difference between them. Folding them together would put a tool-identity switch back inside a
 * class, which is what the migration removes.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_Settings : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
};
