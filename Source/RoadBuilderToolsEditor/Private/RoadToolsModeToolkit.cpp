// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsModeToolkit.h"

#include "EdMode.h"
#include "Framework/Commands/Commands.h"
#include "InteractiveToolManager.h"
#include "RoadToolsMode.h"
#include "RoadToolsModeCommands.h"
#include "Tools/RoadInteractiveTool.h"

#define LOCTEXT_NAMESPACE "RoadToolsModeToolkit"

void FRoadToolsModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	// DIAGNOSTIC (remove when the click path is confirmed): the owning mode is what switches the details
	// views on, so its validity here is the difference between a working property panel and a blank one.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [3 palette] toolkit Init host=%d owningMode=%d"),
		InitToolkitHost.IsValid() ? 1 : 0, InOwningMode.IsValid() ? 1 : 0);

	// Forwarding the owning mode to the base is what switches on the details views and the automatic
	// tool <-> panel wiring, so this override must not be skipped.
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FRoadToolsModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	// The tab strip, in order. The list lives with the commands rather than here because the same names
	// key those commands; the toolkit only presents them.
	//
	// It stays an explicit, ordered list rather than an iteration over GetModeCommands(): the palette
	// order is the UI tab order, and that map is a TMap, whose order is unspecified. The legacy layer
	// kept the ordering as a single source of truth for exactly this reason.
	PaletteNames.Append(FRoadToolsModeCommands::GetPaletteNames());

	// DIAGNOSTIC (remove when the click path is confirmed): the tab list the mode panel is built from.
	// Called once per palette widget rebuild, so this is not a per-frame line.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [3 palette] GetToolPaletteNames count=%d first=%s"),
		PaletteNames.Num(), PaletteNames.Num() > 0 ? *PaletteNames[0].ToString() : TEXT("none"));
}

FText FRoadToolsModeToolkit::GetToolPaletteDisplayName(FName PaletteName) const
{
	// The base returns an empty FText, which renders as a blank tab. The palette names are already the
	// words the user should read, so they are used verbatim.
	return FText::FromName(PaletteName);
}

void FRoadToolsModeToolkit::OnToolPaletteChanged(FName PaletteName)
{
	// A palette switch also selects that palette's tool, which is what makes the File and Settings tabs
	// do their job: they hold no tool button of their own, so the switch itself is the only way in.
	//
	// Every other palette starts on the tool the player expects from that row of buttons, so switching
	// tabs always leaves a working tool active rather than the one from the tab just left.
	if (URoadToolsMode* Mode = Cast<URoadToolsMode>(GetScriptableEditorMode().Get()))
	{
		if (const TCHAR* DefaultTool = URoadToolsMode::GetDefaultToolForPalette(PaletteName))
		{
			Mode->SelectActiveTool(DefaultTool);
		}
	}
}

FName FRoadToolsModeToolkit::GetToolkitFName() const
{
	return FName("RoadToolsMode");
}

FText FRoadToolsModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "Road (New) Toolkit");
}

#undef LOCTEXT_NAMESPACE
