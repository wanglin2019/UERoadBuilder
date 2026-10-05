// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once
#include "Toolkits/BaseToolkit.h"

class SWidget;

/**
 * Toolkit for the road editing mode.
 *
 * Tool palettes (button rows) belong to this class — the FModeToolkit palette protocol is naturally its responsibility:
 * palette listing, button row building and palette switching all go through the registry (GetRoadToolbarDescs / FindRoadToolbarDesc) here.
 *
 * Palette instances are not cached: FRoadToolbar is a stateless builder and button callbacks only capture ERoadToolType values,
 * so BuildToolPalette creates them on the fly; no member container is needed.
 * The inline content panel SRoadEdit only handles detail view layout; this class does not need its definition.
 */
class FRoadEdModeToolkit : public FModeToolkit
{
public:
	/** Initializes the road edit mode toolkit */
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost) override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const;
	virtual void GetToolPaletteNames(TArray<FName>& InPaletteName) const;
	virtual FText GetToolPaletteDisplayName(FName PaletteName) const;
	virtual void BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolbarBuilder);
	virtual void OnToolPaletteChanged(FName PaletteName);
	void SelectParent();
private:
	/** Inline content panel (SRoadEdit instance). Must be cached: GetInlineContent is called repeatedly and must return the same widget. */
	TSharedPtr<SWidget> RoadEdWidget;
};
