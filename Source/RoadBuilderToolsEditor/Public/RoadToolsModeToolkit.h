// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

/**
 * Editor toolkit of URoadToolsMode.
 *
 * Mostly thin: FModeToolkit provides the tool palette (built from the mode's commands) and owns the
 * details views the palette buttons and the active tool's property sets are shown in. UEdMode::Enter()
 * calls the two-argument Init() form, which is what gives the toolkit its owning mode and subscribes
 * its details views to UInteractiveToolManager::OnToolStarted.
 *
 * The one thing it does have to do for itself is put those details views on screen. FModeToolkit only
 * routes its own GetInlineContent() into the mode panel when HasToolkitBuilder() is true, and that
 * needs bUsesToolkitBuilder - a flag nothing in the engine ever sets (it is a leftover of the
 * WidgetRegistration toolkit-builder path, which is a separate system). Left alone, the mode panel
 * renders the palette tabs into ModeToolHeader and nothing else: InlineContentHolder is never given
 * content, so GetInlineContentHolderVisibility() collapses it. The details views are created, are
 * populated by OnToolStarted, and are simply never laid out - which is why every tool, not just the
 * panel-only File and Settings tools, appeared to have lost its property panel.
 *
 * So GetInlineContent() is overridden here to do what the base would have done. The views it returns
 * are the base's own, so the population path stays exactly as the framework intends.
 */
class FRoadToolsModeToolkit : public FModeToolkit
{
public:
	/** FModeToolkit implementation */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const override;
	virtual void OnToolPaletteChanged(FName PaletteName) override;

	/** IToolkit implementation */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

private:
	/**
	 * The widget returned by GetInlineContent(). Built once in Init() and cached.
	 *
	 * Slate calls GetInlineContent() repeatedly and requires the same widget instance each time - a new
	 * one per call would discard the widget the mode panel is already displaying. The legacy toolkit
	 * caches its panel (RoadEdWidget) for exactly this reason.
	 */
	TSharedPtr<SWidget> InlineContent;
};
