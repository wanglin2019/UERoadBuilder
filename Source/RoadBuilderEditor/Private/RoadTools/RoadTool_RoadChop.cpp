#include "RoadTools/RoadTool_RoadChop.h"
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


bool FRoadTool_RoadChop::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	ARoadActor*& SelectedRoad = GetSelectedRoad();
	if (HandleClickRoad(HitProxy, Click))
		return true;
	if (SelectedRoad)
	{
		if (Click.GetKey() == EKeys::RightMouseButton)
		{
			if (HRoadProxy* Proxy = HitProxyCast<HRoadProxy>(HitProxy))
			{
				const FScopedTransaction Transaction(LOCTEXT("RoadChop", "RoadChop"));
				FVector2D UV = SelectedRoad->GetUV(LineTrace(InViewportClient));
				if (SelectedRoad == Proxy->Road)
					SelectedRoad->Chop(UV.X);
				else
					SelectedRoad->Join(Proxy->Road);
				GetScene()->Rebuild();
				return true;
			}
		}
	}
	return false;
}

void FRoadTool_RoadChop::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	DrawRoads(PDI, false);
}