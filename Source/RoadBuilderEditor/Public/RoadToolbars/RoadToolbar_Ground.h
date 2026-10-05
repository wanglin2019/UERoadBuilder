#pragma once
#include "RoadToolbars/RoadToolbar.h"

/** Ground palette: Edit. */
class FRoadToolbar_Ground : public FRoadToolbar
{
public:
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) override;
};
