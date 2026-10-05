#include "RoadToolbars/RoadToolbar_Road.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_Road::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	AddToolButton(ToolBarBuilder, ERoadToolType::RoadPlan, LOCTEXT("RoadEdPlan", "Plan"), LOCTEXT("RoadEdPlanTooltip", "Plan"));
	AddToolButton(ToolBarBuilder, ERoadToolType::RoadHeight, LOCTEXT("RoadEdHeight", "Height"), LOCTEXT("RoadEdHeightTooltip", "Height"));
	AddToolButton(ToolBarBuilder, ERoadToolType::RoadChop, LOCTEXT("RoadEdChop", "Chop"), LOCTEXT("RoadEdChopTooltip", "Chop"));
	AddToolButton(ToolBarBuilder, ERoadToolType::RoadSplit, LOCTEXT("RoadEdSplit", "Split"), LOCTEXT("RoadEdSplitTooltip", "Split"));
}

#undef LOCTEXT_NAMESPACE
