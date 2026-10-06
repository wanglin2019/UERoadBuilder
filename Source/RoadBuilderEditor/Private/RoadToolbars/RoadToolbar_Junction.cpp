#include "RoadToolbars/RoadToolbar_Junction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_Junction::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	AddToolButton(ToolBarBuilder, ERoadToolType::JunctionLink, LOCTEXT("RoadEdLink", "Link"), LOCTEXT("RoadEdLinkTooltip", "Link"));

	// Corner / Mark are placeholders that were never implemented, in any revision (verified against
	// 2895f72, the last commit before the four-layer refactor: ERoadToolType has no such entries and
	// no FRoadTool_JunctionCorner / FRoadTool_JunctionMark class ever existed). The buttons below were
	// already commented out in SRoadEdit.cpp back then. Turning them on is not a restore - it is a
	// from-scratch design task (new enum entries + tool classes).
	//AddToolButton(ToolBarBuilder, ERoadToolType::JunctionCorner, LOCTEXT("RoadEdCorner", "Corner"), LOCTEXT("RoadEdCornerTooltip", "Corner"), FName(TEXT("FoliageEditMode.SetPaintBucket")));
	//AddToolButton(ToolBarBuilder, ERoadToolType::JunctionMark, LOCTEXT("RoadEdMark", "Mark"), LOCTEXT("RoadEdMarkTooltip", "Mark"), FName(TEXT("FoliageEditMode.SetPaintBucket")));
}

#undef LOCTEXT_NAMESPACE
