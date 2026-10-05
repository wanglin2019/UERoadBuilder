#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_RoadPlan : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::RoadPlan; }
	virtual bool ShouldDrawWidget() const { return PointIndex != INDEX_NONE; }
	virtual FVector GetWidgetLocation() const;
	virtual EAxisList::Type GetWidgetAxisToDraw() const { return EAxisList::XY; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange)
	{
		GetSelectedRoad()->Modify();
	}
	virtual void Reset()
	{
		FRoadTool::Reset();
		GetSelectedRoad() = nullptr;
		PointIndex = INDEX_NONE;
	}
	int PointIndex = INDEX_NONE;

private:
	/** Pushes the selected polyline point to the inspector. */
	void ShowPoint(ARoadActor* Road, int Index);
};
