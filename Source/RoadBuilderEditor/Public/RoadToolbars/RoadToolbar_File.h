#pragma once
#include "RoadToolbars/RoadToolbar.h"

/** File palette: no tool buttons yet, only hosts the USettings_File panel. */
class FRoadToolbar_File : public FRoadToolbar
{
public:
	virtual void CreateToolbar(FToolBarBuilder& ToolBarBuilder) override;
};
