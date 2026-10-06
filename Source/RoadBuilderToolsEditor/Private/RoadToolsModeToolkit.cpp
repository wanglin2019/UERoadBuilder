// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsModeToolkit.h"

#include "EdMode.h"
#include "Framework/Commands/Commands.h"
#include "InteractiveToolManager.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "RoadToolsMode.h"
#include "RoadToolsModeCommands.h"
#include "Tools/RoadInteractiveTool.h"

#define LOCTEXT_NAMESPACE "RoadToolsModeToolkit"

void FRoadToolsModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	// Verbose: the owning mode is what switches the details views on, so its validity here is the
	// difference between a working property panel and a blank one.
	RoadLog_Debug(TEXT("toolkit Init host=%d owningMode=%d"),
		InitToolkitHost.IsValid() ? 1 : 0, InOwningMode.IsValid() ? 1 : 0);

	// Forwarding the owning mode to the base is what switches on the details views and the automatic
	// tool <-> panel wiring, so this override must not be skipped.
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	// Both details views now exist (the base built them above), so the container that shows them can be
	// built once and kept. It must be built here rather than inside GetInlineContent(): Slate calls
	// GetInlineContent() repeatedly and expects the same widget back every time, so returning a freshly
	// SNew'd box on each call would throw away the widget the panel already holds. The legacy toolkit
	// caches its panel for the same reason.
	InlineContent =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ModeDetailsView.ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			DetailsView.ToSharedRef()
		];

	// Info: one line per mode entry. If the panel is blank, the thing to check is which of these two
	// views was actually handed a tool by OnToolStarted() - "panel built" and "panel populated" are
	// separate steps and only the second one puts properties on screen.
	RoadLog_Info(TEXT("toolkit inline content built modeDetails=%d details=%d"),
		ModeDetailsView.IsValid() ? 1 : 0, DetailsView.IsValid() ? 1 : 0);
}

TSharedPtr<SWidget> FRoadToolsModeToolkit::GetInlineContent() const
{
	// The base returns an empty TSharedPtr, and the only place its own content would have been wired up
	// (UpdatePrimaryModePanel's HasToolkitBuilder() branch) never runs for a plain FModeToolkit subclass
	// because nothing in the engine sets bUsesToolkitBuilder. So the panel is supplied from here instead.
	//
	// Return the cached widget, never a new one: see the construction in Init().
	return InlineContent;
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

	// Verbose: the tab list the mode panel is built from. Called once per palette widget rebuild, so this
	// is not a per-frame line.
	RoadLog_Debug(TEXT("GetToolPaletteNames count=%d first=%s"),
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
	URoadToolsMode* Mode = Cast<URoadToolsMode>(GetScriptableEditorMode().Get());
	const TCHAR* DefaultTool = URoadToolsMode::GetDefaultToolForPalette(PaletteName);

	// Verbose: the tab switch itself. The pair "palette changed -> tool requested" lives here; the
	// matching "tool started" line comes from the mode. If this never prints, the tab strip is not
	// routing through SetCurrentPalette at all.
	RoadLog_Debug(TEXT("palette changed '%s' mode=%d tool=%s"),
		*PaletteName.ToString(),
		Mode != nullptr ? 1 : 0,
		DefaultTool != nullptr ? DefaultTool : TEXT("none"));

	if (Mode != nullptr && DefaultTool != nullptr)
	{
		Mode->SelectActiveTool(DefaultTool);
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
