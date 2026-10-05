#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_MarkingLane : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::MarkingLane; }
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
		CurrentBoundary->Modify();
	}
	virtual void Reset()
	{
		FRoadTool::Reset();
		CurrentBoundary = nullptr;
		SegmentIndex = INDEX_NONE;
	}
	URoadBoundary* CurrentBoundary = nullptr;
	int SegmentIndex = INDEX_NONE;

private:
	/** Pushes the selected border segment to the inspector. */
	void ShowSegment(URoadBoundary* Boundary, int Index);
};
