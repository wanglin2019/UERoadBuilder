#include "RoadTools/RoadTool_JunctionLink.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadTool_JunctionLink::ShowLink(AJunctionActor* Junction, int Gate, int Link)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Junction || Gate == INDEX_NONE || Link == INDEX_NONE)
	{
		Inspector->ShowStruct(nullptr, nullptr);
		return;
	}
	Inspector->ShowStruct(FJunctionLink::StaticStruct(), &Junction->Gates[Gate].Links[Link],
		[Junction](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			Junction->GetScene()->Rebuild();
		});
}

bool FRoadTool_JunctionLink::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	AJunctionActor*& Junction = GetSelectedJunction();
	if (HandleClickJunction(HitProxy, Click, &GateIndex, &LinkIndex))
	{
		ShowLink(Junction, GateIndex, LinkIndex);
		return true;
	}
	return false;
}

void FRoadTool_JunctionLink::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (AJunctionActor* Junction = GetSelectedJunction())
	{
		for (int i = 0; i < Junction->Gates.Num(); i++)
		{
			FJunctionGate& Gate = Junction->Gates[i];
			for (int j = 0; j < Gate.Links.Num(); j++)
			{
				if (ARoadActor* Road = Gate.Links[j].Road)
				{
					if (URoadCurve* Curve = (j == 1) ? (URoadCurve*)Road->BaseCurve : (URoadCurve*)Road->BaseCurve->RightLane)
					{
						PDI->SetHitProxy(new HJunctionProxy(Junction, i, j));
						if (i == GateIndex && j == LinkIndex)
							DrawCurve(PDI, Curve->Curve, Color_Select, Thickness_Road, DepthBias_Select);
						else
							DrawCurve(PDI, Curve->Curve, Color_Road, Thickness_Road);
					}
				}
			}
		}
	}
	else
		DrawJunctions(PDI);
}

#undef LOCTEXT_NAMESPACE
