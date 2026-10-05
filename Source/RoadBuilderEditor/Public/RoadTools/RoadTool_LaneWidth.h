#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_LaneWidth : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::LaneWidth; }
	virtual bool ShouldDrawWidget() const { return OffsetIndex != INDEX_NONE; }
	virtual FVector GetWidgetLocation() const;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData);
	virtual EAxisList::Type GetWidgetAxisToDraw() const { return EAxisList::XY; }
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
		OffsetIndex = INDEX_NONE;
	}
	URoadBoundary* CurrentBoundary = nullptr;
	int OffsetIndex = INDEX_NONE;

private:
	/** Pushes the selected width control point to the inspector. */
	void ShowOffset(URoadBoundary* Boundary, int Index);
};
