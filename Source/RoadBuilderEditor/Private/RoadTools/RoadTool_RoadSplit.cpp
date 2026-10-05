#include "RoadTools/RoadTool_RoadSplit.h"
#include "RoadEdMode.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"
#include "RoadEdModeToolkit.h"
#include "Toolkits/ToolkitManager.h"

// IMPLEMENT_HIT_PROXY lives only in RoadEdMode.cpp so every tool does not define its own and cause duplicate symbols

#define LOCTEXT_NAMESPACE "RoadBuilder"


bool FRoadTool_RoadSplit::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	ARoadActor*& SelectedRoad = GetSelectedRoad();
	if (HandleClickRoad(HitProxy, Click))
		return true;
	if (SelectedRoad)
	{
		if (Click.GetKey() == EKeys::RightMouseButton)
		{
			if (HRoadCurveProxy* Proxy = HitProxyCast<HRoadCurveProxy>(HitProxy))
			{
				const FScopedTransaction Transaction(LOCTEXT("RoadSplit", "RoadSplit"));
				SelectedRoad->Split(Cast<URoadBoundary>(Proxy->Curve));
				GetScene()->Rebuild();
				return true;
			}
		}
	}
	return false;
}

void FRoadTool_RoadSplit::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		if (SelectedRoad->Length() > 0)
		{
			for (URoadBoundary* Boundary : SelectedRoad->Boundaries)
			{
				PDI->SetHitProxy(new HRoadCurveProxy(Boundary));
				DrawCurve(PDI, Boundary->Curve, Color_Line, Thickness_Line);
			}
		}
	}
	else
		DrawRoads(PDI, false);
}