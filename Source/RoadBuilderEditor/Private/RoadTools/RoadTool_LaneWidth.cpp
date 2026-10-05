#include "RoadTools/RoadTool_LaneWidth.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadTool_LaneWidth::ShowOffset(URoadBoundary* Boundary, int Index)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Boundary || Index == INDEX_NONE)
	{
		Inspector->ShowStruct(nullptr, nullptr);
		return;
	}
	Inspector->ShowStruct(FCurveOffset::StaticStruct(), &Boundary->LocalOffsets[Index],
		[Boundary, Index](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			ARoadActor* Road = Boundary->GetRoad();
			if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FCurveOffset, Dist))
				ClampDist(Boundary->LocalOffsets, Index, Road->Length());
			Road->UpdateLanes();
			Road->GetScene()->Rebuild();
		});
}

FVector FRoadTool_LaneWidth::GetWidgetLocation() const
{
	if (CurrentBoundary && OffsetIndex != INDEX_NONE)
		return CurrentBoundary->GetPos(CurrentBoundary->LocalOffsets[OffsetIndex].Dist);
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_LaneWidth::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (CurrentBoundary && OffsetIndex != INDEX_NONE)
	{
		InMatrix = FRotationMatrix(CurrentBoundary->GetDir(CurrentBoundary->LocalOffsets[OffsetIndex].Dist).Rotation());
		return true;
	}
	return false;
}

bool FRoadTool_LaneWidth::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HandleClickRoad(HitProxy, Click))
		return true;
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		FVector2D UV = SelectedRoad->GetUV(LineTrace(InViewportClient));
		if (Click.GetKey() == EKeys::LeftMouseButton)
		{
			if (HRoadCurveProxy* Proxy = HitProxyCast<HRoadCurveProxy>(HitProxy))
			{
				CurrentBoundary = Cast<URoadBoundary>(Proxy->Curve);
				OffsetIndex = Proxy->Index;
				ShowOffset(CurrentBoundary, OffsetIndex);
			}
			else
				Reset();
			return true;
		}
		if (Click.GetKey() == EKeys::RightMouseButton)
		{
			if (HRoadCurveProxy* Proxy = HitProxyCast<HRoadCurveProxy>(HitProxy))
			{
				const FScopedTransaction Transaction(LOCTEXT("LaneWidth", "LaneWidth"));
				CurrentBoundary = Cast<URoadBoundary>(Proxy->Curve);
				CurrentBoundary->Modify();
				OffsetIndex = CurrentBoundary->AddLocalOffset(UV.X);
				ShowOffset(CurrentBoundary, OffsetIndex);
				return true;
			}
		}
	}
	return false;
}

bool FRoadTool_LaneWidth::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (FRoadTool::InputKey(ViewportClient, Viewport, Key, Event))
		return true;
	if (Event == IE_Pressed && CurrentBoundary && OffsetIndex != INDEX_NONE)
	{
		ARoadActor* SelectedRoad = GetSelectedRoad();
		if (Key == EKeys::Delete)
		{
			CurrentBoundary->DeleteOffset(OffsetIndex);
			OffsetIndex = INDEX_NONE;
			SelectedRoad->UpdateLanes();
			SelectedRoad->GetScene()->Rebuild();
			return true;
		}
	}
	return false;
}

bool FRoadTool_LaneWidth::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (CurrentBoundary)
		{
			FMatrix Mat = GLevelEditorModeTools().GetCustomDrawingCoordinateSystem();
			FVector LocalDrag = Mat.InverseTransformVector(InDrag);
			CurrentBoundary->Modify();
			CurrentBoundary->LocalOffsets[OffsetIndex].Dist += LocalDrag.X;
			CurrentBoundary->LocalOffsets[OffsetIndex].Offset += LocalDrag.Y;
			CurrentBoundary->SnapOffset(OffsetIndex);
			CurrentBoundary->GetRoad()->UpdateLanes();
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_LaneWidth::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		if (SelectedRoad->Length() > 0)
		{
			for (URoadBoundary* Boundary : SelectedRoad->Boundaries)
			{
				if (Boundary != CurrentBoundary)
				{
					PDI->SetHitProxy(new HRoadCurveProxy(Boundary));
					DrawCurve(PDI, Boundary->Curve, Color_Line, Thickness_Line);
				}
			}
			if (CurrentBoundary)
			{
				PDI->SetHitProxy(new HRoadCurveProxy(CurrentBoundary));
				DrawCurve(PDI, CurrentBoundary->Curve, Color_Select, Thickness_Line, DepthBias_Select);
				for (int i = 0; i < CurrentBoundary->LocalOffsets.Num(); i++)
				{
					double Dist = CurrentBoundary->LocalOffsets[i].Dist;
					FColor Color = i == OffsetIndex ? Color_Select : Color_Line;
					PDI->SetHitProxy(new HRoadCurveProxy(CurrentBoundary, i));
					PDI->DrawPoint(CurrentBoundary->GetPos(Dist), Color, Size_Point, SDPG_Foreground);
					if (CurrentBoundary != SelectedRoad->BaseCurve)
					{
						URoadLane* Lane = CurrentBoundary->GetSide() ? CurrentBoundary->RightLane : CurrentBoundary->LeftLane;
						DrawDivider(PDI, Lane, Dist, Color);
					}
				}
			}
		}
	}
	else
		DrawRoads(PDI, false);
}

#undef LOCTEXT_NAMESPACE
