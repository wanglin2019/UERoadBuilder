#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_RoadSplit : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::RoadSplit; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange)
	{
		GetSelectedRoad()->Modify();
	}
};