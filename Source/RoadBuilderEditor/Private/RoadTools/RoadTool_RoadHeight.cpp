#include "RoadTools/RoadTool_RoadHeight.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadTool_RoadHeight::ShowPoint(ARoadActor* Road, int Index)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Road || Index == INDEX_NONE)
	{
		Inspector->ShowStruct(nullptr, nullptr);
		return;
	}
	Inspector->ShowStruct(FHeightPoint::StaticStruct(), &Road->HeightPoints[Index],
		[Road](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			Road->UpdateCurve();
			Road->GetScene()->Rebuild();
		});
}

FVector FRoadTool_RoadHeight::GetWidgetLocation() const
{
	ARoadActor* Road = GetSelectedRoad();
	if (Road && PointIndex != INDEX_NONE)
		return Road->BaseCurve->GetPos(Road->HeightPoints[PointIndex].Dist);
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_RoadHeight::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	ARoadActor* Road = GetSelectedRoad();
	if (Road && PointIndex != INDEX_NONE)
	{
		InMatrix = FRotationMatrix(Road->BaseCurve->GetDir(Road->HeightPoints[PointIndex].Dist).Rotation());
		return true;
	}
	return false;
}

bool FRoadTool_RoadHeight::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	ARoadActor*& SelectedRoad = GetSelectedRoad();
	if (HandleClickRoad(HitProxy, Click, &PointIndex))
	{
		ShowPoint(SelectedRoad, PointIndex);
		return true;
	}
	if (SelectedRoad)
	{
		if (Click.GetKey() == EKeys::RightMouseButton)
		{
			FVector2D UV = SelectedRoad->GetUV(LineTrace(InViewportClient));
			if (HRoadProxy* Proxy = HitProxyCast<HRoadProxy>(HitProxy))
			{
				const FScopedTransaction Transaction(LOCTEXT("RoadHeight", "RoadHeight"));
				SelectedRoad->Modify();
				PointIndex = SelectedRoad->AddHeight(UV.X);
				ShowPoint(SelectedRoad, PointIndex);
				SelectedRoad->UpdateCurve();
				GetScene()->Rebuild();
				return true;
			}
		}
	}
	return false;
}

bool FRoadTool_RoadHeight::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (FRoadTool::InputKey(ViewportClient, Viewport, Key, Event))
		return true;
	if (Event == IE_Pressed)
	{
		if (Key == EKeys::Delete)
		{
			ARoadActor* SelectedRoad = GetSelectedRoad();
			if (SelectedRoad && PointIndex != INDEX_NONE)
			{
				SelectedRoad->HeightPoints.RemoveAt(PointIndex);
				PointIndex = INDEX_NONE;
				GetScene()->Rebuild();
			}
			return true;
		}
	}
	return false;
}

bool FRoadTool_RoadHeight::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (ARoadActor* SelectedRoad = GetSelectedRoad())
		{
			FMatrix Mat = GLevelEditorModeTools().GetCustomDrawingCoordinateSystem();
			FVector LocalDrag = Mat.InverseTransformVector(InDrag);
			SelectedRoad->HeightPoints[PointIndex].Dist += LocalDrag.X;
			SelectedRoad->UpdateCurve();
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_RoadHeight::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	DrawRoads(PDI, false);
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		for (int i = 0; i < SelectedRoad->HeightPoints.Num(); i++)
		{
			PDI->SetHitProxy(new HRoadProxy(SelectedRoad, i));
			PDI->DrawPoint(SelectedRoad->BaseCurve->GetPos(SelectedRoad->HeightPoints[i].Dist), i == PointIndex ? Color_Select : Color_Road, Size_Point, SDPG_Foreground);
		}
	}
}

#undef LOCTEXT_NAMESPACE
