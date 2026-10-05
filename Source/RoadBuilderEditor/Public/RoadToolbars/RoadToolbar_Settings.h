#pragma once
#include "RoadToolbars/RoadToolbar.h"

/** Settings palette: no tool buttons yet, only hosts the USettings_Global panel. */
class FRoadToolbar_Settings : public FRoadToolbar
{
public:
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) override;
};
