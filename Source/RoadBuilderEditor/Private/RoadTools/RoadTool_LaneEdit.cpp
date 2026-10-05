#include "RoadTools/RoadTool_LaneEdit.h"
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

void FRoadTool_LaneEdit::ShowSegment(URoadLane* Lane, int Index)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Lane || Index == INDEX_NONE)
	{
		Inspector->ShowStruct(nullptr, nullptr);
		return;
	}
	Inspector->ShowStruct(FLaneSegment::StaticStruct(), &Lane->Segments[Index],
		[Lane, Index](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			ARoadActor* Road = Lane->GetRoad();
			if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLaneSegment, Dist))
				ClampDist(Lane->Segments, Index, Road->Length());
			else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FLaneSegment, LaneType))
			{
				if (Lane->Segments[Index].LaneType == ELaneType::Collapsed)
				{
					URoadBoundary* Boundary = Lane->GetSide() ? Lane->LeftBoundary : Lane->RightBoundary;
					Boundary->SetZeroOffset(Lane->SegmentStart(Index), Lane->SegmentEnd(Index));
				}
			}
			Road->UpdateLanes();
			Road->GetScene()->Rebuild();
		});
}

FVector FRoadTool_LaneEdit::GetWidgetLocation() const
{
	if (CurrentLane)
		return CurrentLane->GetPos(CurrentLane->SegmentStart(SegmentIndex));
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_LaneEdit::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (CurrentLane)
	{
		InMatrix = FRotationMatrix(CurrentLane->GetDir(CurrentLane->SegmentStart(SegmentIndex)).Rotation());
		return true;
	}
	return false;
}

bool FRoadTool_LaneEdit::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HandleClickRoad(HitProxy, Click))
		return true;
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		FVector2D UV = SelectedRoad->GetUV(LineTrace(InViewportClient));
		URoadLane* Lane = SelectedRoad->GetLane(UV);
		if (Click.GetKey() == EKeys::LeftMouseButton)
		{
			CurrentLane = Lane;
			SegmentIndex = CurrentLane ? CurrentLane->GetSegment(UV.X) : INDEX_NONE;
			ShowSegment(CurrentLane, SegmentIndex);
			return true;
		}
		if (Click.GetKey() == EKeys::RightMouseButton && CurrentLane)
		{
			const FScopedTransaction Transaction(LOCTEXT("LaneEdit", "LaneEdit"));
			if (HRoadCurveProxy* Proxy = HitProxyCast<HRoadCurveProxy>(HitProxy))
			{
				SelectedRoad->CopyLane(CurrentLane, Proxy->Curve == CurrentLane->LeftBoundary);
				SelectedRoad->UpdateLanes();
				GetScene()->Rebuild();
			}
			else if (CurrentLane == Lane)
			{
				CurrentLane->Modify();
				SegmentIndex = CurrentLane->AddSegment(UV.X);
				ShowSegment(CurrentLane, SegmentIndex);
			}
			else
			{
				CurrentLane = Lane;
				SegmentIndex = CurrentLane ? CurrentLane->GetSegment(UV.X) : INDEX_NONE;
				ShowSegment(CurrentLane, SegmentIndex);
			}
			return true;
		}
	}
	return false;
}

bool FRoadTool_LaneEdit::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (FRoadTool::InputKey(ViewportClient, Viewport, Key, Event))
		return true;
	if (Event == IE_Pressed && CurrentLane && SegmentIndex != INDEX_NONE)
	{
		ARoadActor* SelectedRoad = GetSelectedRoad();
		if (Key == EKeys::Home)
		{
			if (CurrentLane->LeftBoundary->LeftLane)
			{
				CurrentLane = CurrentLane->LeftBoundary->LeftLane;
				SegmentIndex = FMath::Min(SegmentIndex, CurrentLane->Segments.Num() - 1);
				ShowSegment(CurrentLane, SegmentIndex);
			}
			return true;
		}
		if (Key == EKeys::End)
		{
			if (CurrentLane->RightBoundary->RightLane)
			{
				CurrentLane = CurrentLane->RightBoundary->RightLane;
				SegmentIndex = FMath::Min(SegmentIndex, CurrentLane->Segments.Num() - 1);
				ShowSegment(CurrentLane, SegmentIndex);
			}
			return true;
		}
		if (Key == EKeys::Tab)
		{
			SegmentIndex = (SegmentIndex + 1) % CurrentLane->Segments.Num();
			ShowSegment(CurrentLane, SegmentIndex);
			return true;
		}
		if (Key == EKeys::Delete)
		{
			CurrentLane->DeleteSegment(SegmentIndex);
			SelectedRoad->UpdateLanes();
			SelectedRoad->GetScene()->Rebuild();
			Reset();
			return true;
		}
	}
	return false;
}

bool FRoadTool_LaneEdit::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (CurrentLane)
		{
			FMatrix Mat = GLevelEditorModeTools().GetCustomDrawingCoordinateSystem();
			FVector LocalDrag = Mat.InverseTransformVector(InDrag);
			CurrentLane->Modify();
			CurrentLane->SegmentStart(SegmentIndex) += LocalDrag.X;
			CurrentLane->SnapSegment(SegmentIndex);
			CurrentLane->GetRoad()->UpdateLanes();
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_LaneEdit::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		if (SelectedRoad->Length() > 0)
		{
			PDI->SetHitProxy(nullptr);
			for (URoadBoundary* Boundary : SelectedRoad->Boundaries)
			{
				if (!CurrentLane || Boundary != CurrentLane->LeftBoundary && Boundary != CurrentLane->RightBoundary)
					DrawCurve(PDI, Boundary->Curve, Color_Line, Thickness_Line);
			}
			for (URoadLane* Lane : SelectedRoad->Lanes)
			{
				for (int i = 0; i < Lane->Segments.Num(); i++)
				{
					double Start = Lane->SegmentStart(i);
					double End = Lane->SegmentEnd(i);
					FColor BoundaryColor = (Lane == CurrentLane && i == SegmentIndex) ? Color_Select : Color_Line;
					FColor Color = (Lane == CurrentLane && (i == SegmentIndex || i - 1 == SegmentIndex)) ? Color_Select : Color_Line;
					if (Lane == CurrentLane)
					{
						PDI->SetHitProxy(new HRoadCurveProxy(Lane->RightBoundary));
						DrawCurve(PDI, Lane->RightBoundary->CreatePolyline(Start, End), BoundaryColor, Thickness_Line, DepthBias_Select);
						PDI->SetHitProxy(new HRoadCurveProxy(Lane->LeftBoundary));
						DrawCurve(PDI, Lane->LeftBoundary->CreatePolyline(Start, End), BoundaryColor, Thickness_Line, DepthBias_Select);
						PDI->SetHitProxy(nullptr);
					}
					DrawDivider(PDI, Lane, Start, Color);
					if (i + 1 == Lane->Segments.Num())
						DrawDivider(PDI, Lane, End, BoundaryColor);
				}
			}
		}
	}
	else
		DrawRoads(PDI, false);
}

#undef LOCTEXT_NAMESPACE
