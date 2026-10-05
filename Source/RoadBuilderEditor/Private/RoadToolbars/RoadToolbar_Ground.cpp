#include "RoadToolbars/RoadToolbar_Ground.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_Ground::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	AddToolButton(ToolBarBuilder, ERoadToolType::GroundEdit, LOCTEXT("RoadEdEdit", "Edit"), LOCTEXT("RoadEdEditTooltip", "Edit"));
}

#undef LOCTEXT_NAMESPACE
