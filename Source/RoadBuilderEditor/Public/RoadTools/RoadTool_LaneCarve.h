#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_LaneCarve : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::LaneCarve; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void Reset()
	{
		FRoadTool::Reset();
		StartUV = FVector2D(-1, 0);
	}
	FVector2D StartUV = FVector2D(-1, 0);
};
