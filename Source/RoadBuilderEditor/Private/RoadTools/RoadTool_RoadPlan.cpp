#include "RoadTools/RoadTool_RoadPlan.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadTool_RoadPlan::ShowPoint(ARoadActor* Road, int Index)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Road || Index == INDEX_NONE)
	{
		Inspector->ShowStruct(nullptr, nullptr);
		return;
	}
	Inspector->ShowStruct(FRoadPoint::StaticStruct(), &Road->RoadPoints[Index],
		[Road](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			Road->UpdateCurve();
			Road->GetScene()->Rebuild();
		});
}

FVector FRoadTool_RoadPlan::GetWidgetLocation() const
{
	ARoadActor* Road = GetSelectedRoad();
	if (Road && PointIndex != INDEX_NONE)
		return Road->GetPos(PointIndex);
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_RoadPlan::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	ARoadActor*& Road = GetSelectedRoad();
	if (HandleClickRoad(HitProxy, Click, &PointIndex))
	{
		ShowPoint(Road, PointIndex);
		return true;
	}
	if (Click.GetKey() == EKeys::LeftMouseButton)
	{
		Reset();
		return true;
	}
	if (Click.GetKey() == EKeys::RightMouseButton)
	{
		const FScopedTransaction Transaction(LOCTEXT("RoadPlan", "RoadPlan"));
		USettings_RoadPlan* Data = GetMutableDefault<USettings_RoadPlan>();
		ARoadScene* Scene = GetScene();
		FRay Ray = GetRay(InViewportClient);
		FVector Pos = FMath::RayPlaneIntersection(Ray.Origin, Ray.Direction, FPlane(FVector(0, 0, Data->BaseHeight), FVector::UpVector));
		if (HRoadProxy* Proxy = HitProxyCast<HRoadProxy>(HitProxy))
		{
			Road = Proxy->Road;
			FVector2D UV = Road->GetUV(Pos);
			Road->Modify();
			Road->AddPoint(UV.X);
		}
		else
		{
			if (!Road)
			{
				Scene->Modify();
				Road = Scene->AddRoad(Data->Style.LoadSynchronous(), Data->BaseHeight);
			}
			Road->Modify();
			Road->InsertPoint((FVector2D&)Pos, PointIndex);
		}
		Road->UpdateCurve();
		Scene->Rebuild();
		ShowPoint(Road, PointIndex);
		return true;
	}
	return false;
}

bool FRoadTool_RoadPlan::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (FRoadTool::InputKey(ViewportClient, Viewport, Key, Event))
		return true;
	if (Event == IE_Pressed)
	{
		if (Key == EKeys::Delete)
		{
			if (ARoadActor* SelectedRoad = GetSelectedRoad())
			{
				ARoadScene* Scene = GetScene();
				if (PointIndex != INDEX_NONE)
				{
					SelectedRoad->RoadPoints.RemoveAt(PointIndex);
					PointIndex = INDEX_NONE;
					SelectedRoad->UpdateCurve();
				}
				else
				{
					Scene->DestroyRoad(SelectedRoad);
					Reset();
				}
				Scene->Rebuild();
			}
			return true;
		}
	}
	return false;
}

bool FRoadTool_RoadPlan::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (ARoadActor* SelectedRoad = GetSelectedRoad())
		{
			SelectedRoad->Modify();
			SelectedRoad->RoadPoints[PointIndex].Pos += (FVector2D&)InDrag;
			if ((PointIndex == 0 || PointIndex == SelectedRoad->RoadPoints.Num() - 1))
			{
				SelectedRoad->DisconnectAll(PointIndex);
				int HeightIndex = PointIndex ? SelectedRoad->HeightPoints.Num() - 1 : 0;
			//	FVector Pos(SelectedRoad->RoadPoints[PointIndex].Pos, SelectedRoad->HeightPoints[HeightIndex].Height);
			//	FVector Location = LineTrace(FRay(InViewportClient->GetViewLocation(), (Pos - InViewportClient->GetViewLocation()).GetSafeNormal()));
				ARoadActor* HoveredRoad = GetScene()->PickRoad(FVector(SelectedRoad->RoadPoints[PointIndex].Pos, SelectedRoad->HeightPoints[HeightIndex].Height), SelectedRoad);
				if (HoveredRoad && HoveredRoad != SelectedRoad)
				{
				//	SelectedRoad->HeightPoints[HeightIndex].Height = Location.Z;
					SelectedRoad->ConnectTo(PointIndex, HoveredRoad);
				}
			}
			SelectedRoad->UpdateCurve();
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_RoadPlan::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	DrawRoads(PDI, false);
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		for (int i = 0; i < SelectedRoad->RoadPoints.Num(); i++)
		{
			PDI->SetHitProxy(new HRoadProxy(SelectedRoad, i));
			PDI->DrawPoint(SelectedRoad->GetPos(i), i == PointIndex ? Color_Select : Color_Road, Size_Point, SDPG_Foreground);
		}
	}
}

#undef LOCTEXT_NAMESPACE
