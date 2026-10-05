#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_Settings : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::Settings; }
};