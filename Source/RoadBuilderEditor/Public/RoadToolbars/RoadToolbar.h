#pragma once
#include "CoreMinimal.h"
#include "RoadTools/RoadToolType.h"

class FToolBarBuilder;

/** Palette names. Defined in RoadToolbar.cpp. */
extern FName PaletteName_File;
extern FName PaletteName_Road;
extern FName PaletteName_Junction;
extern FName PaletteName_Lane;
extern FName PaletteName_Marking;
extern FName PaletteName_Ground;
extern FName PaletteName_Settings;

/**
 * One palette corresponds to one button row.
 * Only builds buttons: owns no panel, no mode, and never touches any detail view.
 * Button callbacks resolve FEdModeRoad::Get() at click time, so this class knows nothing about its hosts.
 *
 * Instances are managed by FRoadEdModeToolkit by palette name (the palette protocol belongs to it),
 * neither SRoadEdit nor FEdModeRoad knows about this class.
 */
class FRoadToolbar
{
public:
	virtual ~FRoadToolbar() = default;

	/** Subclasses add their own buttons here. */
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) = 0;

protected:
	/** Shared button construction: binds by identity to FEdModeRoad::SetCurrentToolByType / GetCurrentToolType. */
	void AddToolButton(FToolBarBuilder& ToolBarBuilder, ERoadToolType ToolType, const FText& Label, const FText& Tooltip, const FName& IconName = FName(TEXT("FoliageEditMode.SetSelect")));
};

/**
 * Palette descriptor table. The single source of the palette list:
 * palette name list, default tool on palette switch and button row creation all derive from it.
 */
struct FRoadToolbarDesc
{
	FName PaletteName;
	ERoadToolType DefaultTool;
	TFunction<TUniquePtr<FRoadToolbar>()> Factory;
};

/** All palette descriptors (order defines panel display order). */
const TArray<FRoadToolbarDesc>& GetRoadToolbarDescs();

/** Finds by name; returns nullptr if not found. */
const FRoadToolbarDesc* FindRoadToolbarDesc(FName PaletteName);
