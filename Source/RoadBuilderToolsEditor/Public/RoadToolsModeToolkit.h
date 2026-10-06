// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

/**
 * Editor toolkit of URoadToolsMode.
 *
 * Deliberately thin. FModeToolkit already provides the whole mode panel: it builds the tool palette
 * from the mode's commands and owns the details views that display the active tool's property sets.
 * UEdMode::Enter() calls the two-argument Init() form, which is what makes those details views work -
 * the legacy single-argument path left the owning mode null and the panels permanently empty, which is
 * why the legacy mode had to hand-build its own inspector. Nothing of that is needed here.
 */
class FRoadToolsModeToolkit : public FModeToolkit
{
public:
	/** FModeToolkit implementation */
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const override;
	virtual void OnToolPaletteChanged(FName PaletteName) override;

	/** IToolkit implementation */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
};
