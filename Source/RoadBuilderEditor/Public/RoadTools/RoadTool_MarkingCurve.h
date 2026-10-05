#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_MarkingCurve : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::MarkingCurve; }
	virtual bool ShouldDrawWidget() const { return CurrentMarking != nullptr; }
	virtual FVector GetWidgetLocation() const;
	virtual bool GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData);
	virtual EAxisList::Type GetWidgetAxisToDraw() const { return EAxisList::XY; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange)
	{
		// Reachable with nothing selected: the inspector pushes property edits back through the mode at any time.
		if (CurrentMarking)
			CurrentMarking->Modify();
	}
	virtual void Reset()
	{
		FRoadTool::Reset();
		CurrentMarking = nullptr;
		PointIndex = INDEX_NONE;
		SubIndex = 0;
	}
	UMarkingCurve* CurrentMarking = nullptr;
	int PointIndex = INDEX_NONE;
	int SubIndex = 0;

private:
	/** Shows the whole curve when Index is INDEX_NONE, otherwise a single control point. */
	void ShowMarking(UMarkingCurve* Marking, int Index);
};
