#pragma once
#include "RoadToolbars/RoadToolbar.h"

/** Junction palette: Link (Corner / Mark not enabled yet, see the .cpp comment). */
class FRoadToolbar_Junction : public FRoadToolbar
{
public:
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) override;
};
