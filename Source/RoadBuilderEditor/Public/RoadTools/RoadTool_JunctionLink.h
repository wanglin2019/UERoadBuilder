#pragma once
#include "RoadTools/RoadTool.h"

class FRoadTool_JunctionLink : public FRoadTool
{
public:
	virtual ERoadToolType GetToolType() const override { return ERoadToolType::JunctionLink; }
	virtual bool HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange)
	{
		GetSelectedJunction()->Gates[GateIndex].Links[LinkIndex].Road->Modify();
	}
	virtual void Reset()
	{
		FRoadTool::Reset();
		GateIndex = INDEX_NONE;
		LinkIndex = INDEX_NONE;
	}
	int GateIndex = INDEX_NONE;
	int LinkIndex = INDEX_NONE;

private:
	/** Pushes the selected junction link to the inspector. */
	void ShowLink(AJunctionActor* Junction, int Gate, int Link);
};
