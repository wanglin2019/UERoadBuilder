// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsModeToolkit.h"

#include "Framework/Commands/Commands.h"

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
	// S0 has a single palette, so an explicit list is both the list and its order.
	//
	// S3 note: with the seven palettes the plugin needs, this must stay an explicit, ordered list.
	// The palette order is the UI tab order, and the alternative - iterating GetModeCommands() - walks
	// a TMap, whose order is unspecified. The legacy layer kept that ordering as a single source of
	// truth for exactly this reason.
	PaletteNames.Add(NAME_Default);

	// DIAGNOSTIC (remove when the click path is confirmed): the tab list the mode panel is built from.
	// Called once per palette widget rebuild, so this is not a per-frame line.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [3 palette] GetToolPaletteNames count=%d first=%s"),
		PaletteNames.Num(), PaletteNames.Num() > 0 ? *PaletteNames[0].ToString() : TEXT("none"));
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
