#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_GroundEdit : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::GroundEdit; }
	virtual bool ShouldDrawWidget() const { return PointIndex != INDEX_NONE && GetSelectedGround()->Points[PointIndex].Road == nullptr; }
	virtual FVector GetWidgetLocation() const;
	virtual EAxisList::Type GetWidgetAxisToDraw() const { return EAxisList::XY; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void Reset()
	{
		FRoadTool::Reset();
		GetSelectedGround() = nullptr;
		PointIndex = INDEX_NONE;
	}
	int PointIndex = INDEX_NONE;

private:
	/** Pushes the selected ground point to the inspector: road-following points show FGroundPoint, manual points show FVector. */
	void ShowPoint(AGroundActor* Ground, int Index);
};
