#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_LaneEdit : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::LaneEdit; }
	virtual bool ShouldDrawWidget() const { return SegmentIndex != INDEX_NONE; }
	virtual FVector GetWidgetLocation() const;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData);
	virtual EAxisList::Type GetWidgetAxisToDraw() const { return EAxisList::X; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange)
	{
		CurrentLane->Modify();
	}
	virtual void Reset()
	{
		FRoadTool::Reset();
		CurrentLane = nullptr;
		SegmentIndex = INDEX_NONE;
	}
	URoadLane* CurrentLane = nullptr;
	int SegmentIndex = INDEX_NONE;

private:
	/** Pushes the selected lane segment to the inspector. */
	void ShowSegment(URoadLane* Lane, int Index);
};
