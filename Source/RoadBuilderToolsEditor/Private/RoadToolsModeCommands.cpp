// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsModeCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "RoadToolsModeCommands"

FRoadToolsModeCommands::FRoadToolsModeCommands()
	: TCommands<FRoadToolsModeCommands>(
		TEXT("RoadToolsMode"),
		NSLOCTEXT("RoadToolsMode", "RoadToolsModeCommands", "Road Tools Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FRoadToolsModeCommands::RegisterCommands()
{
	// S0 has one palette only. S3 adds the remaining six, each under its own palette name, and the
	// toolkit's GetToolPaletteNames() must list them in the order the user should see them.
	TArray<TSharedPtr<FUICommandInfo>>& RoadToolsModeCommands = Commands.FindOrAdd(NAME_Default);

	UI_COMMAND(RoadPlan, "Road Plan", "Click to place road alignment points", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(RoadPlan);

	UI_COMMAND(RoadHeight, "Road Height", "Edit the height profile of the selected road", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(RoadHeight);

	UI_COMMAND(RoadChop, "Road Chop", "Chop the selected road, or join another road onto it", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(RoadChop);

	UI_COMMAND(RoadSplit, "Road Split", "Split the selected road along one of its boundaries", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(RoadSplit);

	UI_COMMAND(LaneCarve, "Lane Carve", "Carve a lane transition between two points on a boundary", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(LaneCarve);

	UI_COMMAND(LaneEdit, "Lane Edit", "Move a lane segment, or copy a lane across a boundary", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(LaneEdit);

	UI_COMMAND(LaneWidth, "Lane Width", "Widen or narrow the road by editing its width control points", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(LaneWidth);

	UI_COMMAND(MarkingLane, "Marking Lane", "Edit the lane markings on a road's boundary segments", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(MarkingLane);

	UI_COMMAND(MarkingPoint, "Marking Point", "Place and edit point markings on a road", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(MarkingPoint);

	UI_COMMAND(MarkingCurve, "Marking Curve", "Draw and edit spline markings on a road", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(MarkingCurve);

	UI_COMMAND(GroundEdit, "Ground Edit", "Edit the outline a ground area is built from", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(GroundEdit);

	UI_COMMAND(JunctionLink, "Junction Link", "Inspect and tune the link curves a junction generates", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(JunctionLink);

	UI_COMMAND(File, "File", "Export the road network", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(File);

	UI_COMMAND(Settings, "Settings", "Global road building settings", EUserInterfaceActionType::ToggleButton, FInputChord());
	RoadToolsModeCommands.Add(Settings);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FRoadToolsModeCommands::GetCommands()
{
	return FRoadToolsModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
