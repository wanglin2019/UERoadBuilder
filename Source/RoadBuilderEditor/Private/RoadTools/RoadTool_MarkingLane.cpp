#include "RoadTools/RoadTool_MarkingLane.h"
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

void FRoadTool_MarkingLane::ShowSegment(URoadBoundary* Boundary, int Index)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Boundary || Index == INDEX_NONE)
	{
		Inspector->ShowStruct(nullptr, nullptr);
		return;
	}
	Inspector->ShowStruct(FBoundarySegment::StaticStruct(), &Boundary->Segments[Index],
		[Boundary, Index](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			ARoadActor* Road = Boundary->GetRoad();
			if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FBoundarySegment, Dist))
				ClampDist(Boundary->Segments, Index, Road->Length());
			Road->UpdateLanes();
			Road->GetScene()->Rebuild();
		});
}

FVector FRoadTool_MarkingLane::GetWidgetLocation() const
{
	if (CurrentBoundary)
		return CurrentBoundary->GetPos(CurrentBoundary->SegmentStart(SegmentIndex));
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_MarkingLane::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (CurrentBoundary)
	{
		InMatrix = FRotationMatrix(CurrentBoundary->GetDir(CurrentBoundary->SegmentStart(SegmentIndex)).Rotation());
		return true;
	}
	return false;
}

bool FRoadTool_MarkingLane::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
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
				SegmentIndex = Proxy->Index;
				ShowSegment(CurrentBoundary, SegmentIndex);
			}
			else
				Reset();
			return true;
		}
		if (Click.GetKey() == EKeys::RightMouseButton)
		{
			if (HRoadCurveProxy* Proxy = HitProxyCast<HRoadCurveProxy>(HitProxy))
			{
				const FScopedTransaction Transaction(LOCTEXT("MarkingLane", "MarkingLane"));
				CurrentBoundary = Cast<URoadBoundary>(Proxy->Curve);
				CurrentBoundary->Modify();
				SegmentIndex = CurrentBoundary->AddSegment(UV.X);
				ShowSegment(CurrentBoundary, SegmentIndex);
				return true;
			}
		}
	}
	return false;
}

bool FRoadTool_MarkingLane::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (FRoadTool::InputKey(ViewportClient, Viewport, Key, Event))
		return true;
	if (Event == IE_Pressed && CurrentBoundary && SegmentIndex != INDEX_NONE)
	{
		ARoadActor* SelectedRoad = GetSelectedRoad();
		if (Key == EKeys::Home)
		{
			if (CurrentBoundary->LeftLane->LeftBoundary)
			{
				CurrentBoundary = CurrentBoundary->LeftLane->LeftBoundary;
				SegmentIndex = FMath::Min(SegmentIndex, CurrentBoundary->Segments.Num() - 1);
				ShowSegment(CurrentBoundary, SegmentIndex);
			}
			return true;
		}
		if (Key == EKeys::End)
		{
			if (CurrentBoundary->RightLane->RightBoundary)
			{
				CurrentBoundary = CurrentBoundary->RightLane->RightBoundary;
				SegmentIndex = FMath::Min(SegmentIndex, CurrentBoundary->Segments.Num() - 1);
				ShowSegment(CurrentBoundary, SegmentIndex);
			}
			return true;
		}
		if (Key == EKeys::Tab)
		{
			SegmentIndex = (SegmentIndex + 1) % CurrentBoundary->Segments.Num();
			ShowSegment(CurrentBoundary, SegmentIndex);
			return true;
		}
		if (Key == EKeys::Delete)
		{
			CurrentBoundary->DeleteSegment(SegmentIndex);
			SelectedRoad->UpdateLanes();
			SelectedRoad->GetScene()->Rebuild();
			Reset();
			return true;
		}
	}
	return false;
}

bool FRoadTool_MarkingLane::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (CurrentBoundary)
		{
			FMatrix Mat = GLevelEditorModeTools().GetCustomDrawingCoordinateSystem();
			FVector LocalDrag = Mat.InverseTransformVector(InDrag);
			CurrentBoundary->Modify();
			CurrentBoundary->SegmentStart(SegmentIndex) += LocalDrag.X;
			CurrentBoundary->SnapSegment(SegmentIndex);
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_MarkingLane::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		if (SelectedRoad->Length() > 0)
		{
			for (URoadBoundary* Boundary : SelectedRoad->Boundaries)
			{
				for (int i = 0; i < Boundary->Segments.Num(); i++)
				{
					if (Boundary != SelectedRoad->BaseCurve && Boundary->IsZeroOffset(i))
						continue;
					double Start = Boundary->SegmentStart(i);
					double End = Boundary->SegmentEnd(i);
					PDI->SetHitProxy(new HRoadCurveProxy(Boundary, i));
					FColor Color = (Boundary == CurrentBoundary && i == SegmentIndex) ? Color_Select : Color_Line;
					float DepthBias = (Boundary == CurrentBoundary && i == SegmentIndex) ? DepthBias_Select : 0;
					DrawCurve(PDI, Boundary->CreatePolyline(Start, End), Color, Thickness_Line, DepthBias);
					DrawPoint(PDI, Boundary, Start, Color);
					if (i + 1 == Boundary->Segments.Num())
						DrawPoint(PDI, Boundary, End, Color);
				}
			}
		}
	}
	else
		DrawRoads(PDI, true);
}

#undef LOCTEXT_NAMESPACE
