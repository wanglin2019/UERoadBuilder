#include "RoadToolbars/RoadToolbar_Junction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_Junction::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	AddToolButton(ToolBarBuilder, ERoadToolType::JunctionLink, LOCTEXT("RoadEdLink", "Link"), LOCTEXT("RoadEdLinkTooltip", "Link"));

	// The tools for Corner / Mark were removed in the refactor; to restore them, add the enum entries to ERoadToolType plus the matching tool classes.
	//AddToolButton(ToolBarBuilder, ERoadToolType::JunctionCorner, LOCTEXT("RoadEdCorner", "Corner"), LOCTEXT("RoadEdCornerTooltip", "Corner"), FName(TEXT("FoliageEditMode.SetPaintBucket")));
	//AddToolButton(ToolBarBuilder, ERoadToolType::JunctionMark, LOCTEXT("RoadEdMark", "Mark"), LOCTEXT("RoadEdMarkTooltip", "Mark"), FName(TEXT("FoliageEditMode.SetPaintBucket")));
}

#undef LOCTEXT_NAMESPACE
