#include "RoadToolbars/RoadToolbar_Marking.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_Marking::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	AddToolButton(ToolBarBuilder, ERoadToolType::MarkingLane, LOCTEXT("RoadEdLane", "Lane"), LOCTEXT("RoadEdLaneTooltip", "Lane"));
	AddToolButton(ToolBarBuilder, ERoadToolType::MarkingPoint, LOCTEXT("RoadEdPoint", "Point"), LOCTEXT("RoadEdPointTooltip", "Point"));
	AddToolButton(ToolBarBuilder, ERoadToolType::MarkingCurve, LOCTEXT("RoadEdCurve", "Curve"), LOCTEXT("RoadEdCurveTooltip", "Curve"));
}

#undef LOCTEXT_NAMESPACE
