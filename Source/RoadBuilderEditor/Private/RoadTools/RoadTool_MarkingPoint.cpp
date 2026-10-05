#include "RoadTools/RoadTool_MarkingPoint.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadTool_MarkingPoint::ShowMarking(UMarkingPoint* Marking)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Marking)
	{
		Inspector->ShowObject(nullptr);
		return;
	}
	Inspector->ShowObject(Marking,
		[Marking](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			Marking->GetRoad()->GetScene()->Rebuild();
		});
}

FVector FRoadTool_MarkingPoint::GetWidgetLocation() const
{
	if (CurrentMarking)
		return CurrentMarking->GetRoad()->GetPos(CurrentMarking->Point);
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_MarkingPoint::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (CurrentMarking)
	{
		InMatrix = FRotationMatrix(CurrentMarking->GetRoad()->GetDir(CurrentMarking->Point.X).Rotation());
		return true;
	}
	return false;
}

bool FRoadTool_MarkingPoint::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HandleClickRoad(HitProxy, Click))
		return true;
	ARoadActor* SelectedRoad = GetSelectedRoad();
	if (Click.GetKey() == EKeys::LeftMouseButton)
	{
		if (HRoadMarkingProxy* Proxy = HitProxyCast<HRoadMarkingProxy>(HitProxy))
		{
			CurrentMarking = Cast<UMarkingPoint>(Proxy->Marking);
			ShowMarking(CurrentMarking);
		}
		else
			Reset();
		return true;
	}
	if (Click.GetKey() == EKeys::RightMouseButton)
	{
		if (SelectedRoad)
		{
			const FScopedTransaction Transaction(LOCTEXT("MarkingPoint", "MarkingPoint"));
			SelectedRoad->Modify();
			CurrentMarking = SelectedRoad->AddMarkingPoint(SelectedRoad->GetUV(LineTrace(InViewportClient)));
			ShowMarking(CurrentMarking);
		}
		return true;
	}
	return false;
}

bool FRoadTool_MarkingPoint::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (FRoadTool::InputKey(ViewportClient, Viewport, Key, Event))
		return true;
	if (Event == IE_Pressed && CurrentMarking)
	{
		if (Key == EKeys::Delete)
		{
			ARoadActor* SelectedRoad = GetSelectedRoad();
			SelectedRoad->DeleteMarking(CurrentMarking);
			// Reset() clears CurrentMarking and empties the inspector; without it the panel keeps the deleted object.
			Reset();
			SelectedRoad->GetScene()->Rebuild();
			return true;
		}
	}
	return false;
}

bool FRoadTool_MarkingPoint::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (CurrentMarking)
		{
			FMatrix Mat = GLevelEditorModeTools().GetCustomDrawingCoordinateSystem();
			FVector LocalDrag = Mat.InverseTransformVector(InDrag);
			CurrentMarking->Modify();
			CurrentMarking->Point += (FVector2D&)LocalDrag;
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_MarkingPoint::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	auto Draw = [&](ARoadActor* Road)
	{
		for (URoadMarking* Marking : Road->Markings)
		{
			if (UMarkingPoint* MarkingPoint = Cast<UMarkingPoint>(Marking))
			{
				PDI->SetHitProxy(new HRoadMarkingProxy(MarkingPoint));
				PDI->DrawPoint(MarkingPoint->GetRoad()->GetPos(MarkingPoint->Point), Marking == CurrentMarking ? Color_Select : Color_Line, Size_Point, SDPG_Foreground);
			}
		}
	};
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
		Draw(SelectedRoad);
	else
		DrawRoads(PDI, true);
}

#undef LOCTEXT_NAMESPACE
