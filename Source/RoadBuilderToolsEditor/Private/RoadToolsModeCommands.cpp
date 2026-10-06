// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsModeCommands.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "RoadToolsModeCommands"

const FName PaletteName_File = TEXT("File");
const FName PaletteName_Road = TEXT("Road");
const FName PaletteName_Junction = TEXT("Junction");
const FName PaletteName_Lane = TEXT("Lane");
const FName PaletteName_Marking = TEXT("Marking");
const FName PaletteName_Ground = TEXT("Ground");
const FName PaletteName_Settings = TEXT("Settings");

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
	// One entry per palette, keyed by the palette's name. Each palette's array is also its button
	// order, top to bottom, so the two things the user sees - which tab a button lives under and where
	// it sits in that tab - are decided here and nowhere else.
	//
	// The split mirrors the legacy layer's seven toolbars one for one (RoadToolbars/RoadToolbar_*.cpp),
	// including which tools share a row. The two palettes whose legacy counterpart held no buttons at
	// all (File, Settings) still exist as palettes: they have no tool of their own to pick, and their
	// purpose is to put that panel's settings in front of the user, which here means switching to the
	// File / Settings tool - see FRoadToolsModeToolkit::OnToolPaletteChanged.
	//
	// The names below are the palette identifiers, and FRoadToolsModeCommands::GetPaletteNames() lists
	// them in UI order; the toolkit reads that list rather than this map, because a TMap has no order.

	// File: panel-only. Switching to this palette starts the File tool.
	//
	// The command is created but deliberately kept OUT of the palette array: UEdMode::RegisterTool()
	// maps a command to "start this tool" and needs a real one, while the palette array is only what
	// BuildToolPalette() iterates to draw buttons. An empty array is therefore a valid palette with no
	// buttons - which is exactly the legacy shape (RoadToolbar_File draws nothing) - but the command
	// still has to exist or RegisterTool() is handed a null and asserts in FUICommandList::MapAction.
	{
		UI_COMMAND(File, "File", "Import and export the road network", EUserInterfaceActionType::Button, FInputChord());
		Commands.Add(PaletteName_File, {});
	}

	// Road: the alignment and the coarse road-level operations.
	{
		TArray<TSharedPtr<FUICommandInfo>>& Palette = Commands.Add(PaletteName_Road);
		UI_COMMAND(RoadPlan, "Road Plan", "Click to place road alignment points", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(RoadPlan);
		UI_COMMAND(RoadHeight, "Road Height", "Edit the height profile of the selected road", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(RoadHeight);
		UI_COMMAND(RoadChop, "Road Chop", "Chop the selected road, or join another road onto it", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(RoadChop);
		UI_COMMAND(RoadSplit, "Road Split", "Split the selected road along one of its boundaries", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(RoadSplit);
	}

	// Junction.
	{
		TArray<TSharedPtr<FUICommandInfo>>& Palette = Commands.Add(PaletteName_Junction);
		UI_COMMAND(JunctionLink, "Junction Link", "Inspect and tune the link curves a junction generates", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(JunctionLink);
	}

	// Lane.
	{
		TArray<TSharedPtr<FUICommandInfo>>& Palette = Commands.Add(PaletteName_Lane);
		UI_COMMAND(LaneEdit, "Lane Edit", "Move a lane segment, or copy a lane across a boundary", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(LaneEdit);
		UI_COMMAND(LaneCarve, "Lane Carve", "Carve a lane transition between two points on a boundary", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(LaneCarve);
		UI_COMMAND(LaneWidth, "Lane Width", "Widen or narrow the road by editing its width control points", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(LaneWidth);
	}

	// Marking.
	{
		TArray<TSharedPtr<FUICommandInfo>>& Palette = Commands.Add(PaletteName_Marking);
		UI_COMMAND(MarkingLane, "Marking Lane", "Edit the lane markings on a road's boundary segments", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(MarkingLane);
		UI_COMMAND(MarkingPoint, "Marking Point", "Place and edit point markings on a road", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(MarkingPoint);
		UI_COMMAND(MarkingCurve, "Marking Curve", "Draw and edit spline markings on a road", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(MarkingCurve);
	}

	// Ground.
	{
		TArray<TSharedPtr<FUICommandInfo>>& Palette = Commands.Add(PaletteName_Ground);
		UI_COMMAND(GroundEdit, "Ground Edit", "Edit the outline a ground area is built from", EUserInterfaceActionType::ToggleButton, FInputChord());
		Palette.Add(GroundEdit);
	}

	// Settings: panel-only, same as File.
	{
		UI_COMMAND(Settings, "Settings", "Global road network settings", EUserInterfaceActionType::Button, FInputChord());
		Commands.Add(PaletteName_Settings, {});
	}
}

const TArray<FName>& FRoadToolsModeCommands::GetPaletteNames()
{
	// The palette order as the UI should show it. Held in one function-local static array, so this is
	// the only place that decides tab order: the toolkit reads it for the tab list, and the mode
	// registers each tool against the commands stored under the same name.
	//
	// Legacy kept the same ordering as a single source of truth and for the same reason - the
	// alternative, walking the palette map, would follow TMap iteration order, which is not defined.
	// Keep this list in step with the palettes that RegisterCommands() adds.
	static const TArray<FName> PaletteNames =
	{
		PaletteName_File,
		PaletteName_Road,
		PaletteName_Junction,
		PaletteName_Lane,
		PaletteName_Marking,
		PaletteName_Ground,
		PaletteName_Settings,
	};
	return PaletteNames;
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FRoadToolsModeCommands::GetCommands()
{
	return FRoadToolsModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
