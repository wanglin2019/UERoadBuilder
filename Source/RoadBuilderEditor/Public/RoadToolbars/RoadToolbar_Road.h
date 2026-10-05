#pragma once
#include "RoadToolbars/RoadToolbar.h"

/** Road palette: Plan / Height / Chop / Split. */
class FRoadToolbar_Road : public FRoadToolbar
{
public:
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) override;
};
