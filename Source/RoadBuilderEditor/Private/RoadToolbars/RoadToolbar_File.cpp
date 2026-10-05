#include "RoadToolbars/RoadToolbar_File.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadToolbar_File::CreateToolbar(FToolBarBuilder& ToolBarBuilder)
{
	// The File palette has no tool buttons: switching to it drives the content panel via FEdModeRoad::SetCurrentToolByType(ERoadToolType::File).
}

#undef LOCTEXT_NAMESPACE
