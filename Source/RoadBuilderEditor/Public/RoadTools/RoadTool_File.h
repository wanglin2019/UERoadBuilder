#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_File : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::File; }
};