#include "RoadToolbars/RoadToolbar_Lane.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_Lane::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	AddToolButton(ToolBarBuilder, ERoadToolType::LaneEdit, LOCTEXT("RoadEdEdit", "Edit"), LOCTEXT("RoadEdEditTooltip", "Edit"));
	AddToolButton(ToolBarBuilder, ERoadToolType::LaneCarve, LOCTEXT("RoadEdCarve", "Carve"), LOCTEXT("RoadEdCarveTooltip", "Carve"));
	AddToolButton(ToolBarBuilder, ERoadToolType::LaneWidth, LOCTEXT("RoadEdWidth", "Width"), LOCTEXT("RoadEdWidthTooltip", "Width"));
}

#undef LOCTEXT_NAMESPACE
