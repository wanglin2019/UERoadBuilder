#pragma once
#include "RoadToolbars/RoadToolbar.h"

/** Lane palette: Edit / Carve / Width. */
class FRoadToolbar_Lane : public FRoadToolbar
{
public:
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) override;
};
