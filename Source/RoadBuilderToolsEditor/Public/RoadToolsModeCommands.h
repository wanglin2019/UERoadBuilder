// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * Commands of the new road editing mode.
 *
 * These carry the double duty that makes the UEdMode framework click: UI_COMMAND builds the button,
 * and UEdMode::RegisterTool() maps the same command to "start tool X". The palette that UEdMode hands
 * back from GetModeCommands() is exactly this map, keyed by palette name, so the buttons the user sees
 * and the tools that can be started cannot drift apart.
 *
 * The command set name ("RoadToolsMode") is deliberately different from the legacy
 * FRoadBuilderEditorCommands ("RoadBuilderEditor"): two TCommands sets in one plugin with the same name
 * would collide, and the legacy Esc binding must keep working while both modes coexist.
 */
class FRoadToolsModeCommands : public TCommands<FRoadToolsModeCommands>
{
public:
	FRoadToolsModeCommands();

	/** TCommands implementation */
	virtual void RegisterCommands() override;

	/** Palette name -> commands, as consumed by UEdMode::GetModeCommands(). */
	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands();

	/** Drops road alignment points. */
	TSharedPtr<FUICommandInfo> RoadPlan;

	/** Edits the height profile of the selected road. S0.5 pilot: the hardest legacy tool to port. */
	TSharedPtr<FUICommandInfo> RoadHeight;

	/** Exports the road network. Panel-only. */
	TSharedPtr<FUICommandInfo> File;

	/** Global settings. Panel-only. */
	TSharedPtr<FUICommandInfo> Settings;

	/** Chops the selected road, or joins another road onto it. */
	TSharedPtr<FUICommandInfo> RoadChop;

	/** Splits the selected road along one of its boundaries. */
	TSharedPtr<FUICommandInfo> RoadSplit;

	/** Carves a lane transition between two points on a boundary. */
	TSharedPtr<FUICommandInfo> LaneCarve;

	/** Picks a lane segment and moves it, or copies a lane across a boundary. */
	TSharedPtr<FUICommandInfo> LaneEdit;

	/** Widens or narrows a road by editing the width control points along a boundary. */
	TSharedPtr<FUICommandInfo> LaneWidth;

	/** Edits the lane markings carried by a road's boundary segments. */
	TSharedPtr<FUICommandInfo> MarkingLane;

	/** Places and edits point markings on a road. */
	TSharedPtr<FUICommandInfo> MarkingPoint;

	/** Draws and edits spline markings on a road. */
	TSharedPtr<FUICommandInfo> MarkingCurve;

	/** Edits the outline a ground area is built from. */
	TSharedPtr<FUICommandInfo> GroundEdit;

	/** Inspects and tunes the link curves a junction generates. */
	TSharedPtr<FUICommandInfo> JunctionLink;

protected:
	/** Filled in RegisterCommands(), read back through GetCommands(). */
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};
