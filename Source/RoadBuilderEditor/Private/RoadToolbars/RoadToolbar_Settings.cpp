#include "RoadToolbars/RoadToolbar_Settings.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_Settings::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	// The Settings palette has no tool buttons: switching to it drives the content panel via FEdModeRoad::SetCurrentToolByType(ERoadToolType::Settings).
}

#undef LOCTEXT_NAMESPACE
