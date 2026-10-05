#pragma once
#include "RoadToolbars/RoadToolbar.h"

/** Marking palette: Lane / Point / Curve. */
class FRoadToolbar_Marking : public FRoadToolbar
{
public:
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) override;
};
