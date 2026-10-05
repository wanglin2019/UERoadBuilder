#include "RoadTools/RoadTool_MarkingCurve.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadTool_MarkingCurve::ShowMarking(UMarkingCurve* Marking, int Index)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Marking)
	{
		Inspector->Clear();
		return;
	}

	// Whole curve shows the object; a single control point shows the struct
	if (Index == INDEX_NONE)
	{
		Inspector->ShowObject(Marking,
			[Marking](const FPropertyChangedEvent& PropertyChangedEvent)
			{
				Marking->GetRoad()->GetScene()->Rebuild();
			});
	}
	else
	{
		Inspector->ShowStruct(FMarkingCurvePoint::StaticStruct(), &Marking->Points[Index],
			[Marking](const FPropertyChangedEvent& PropertyChangedEvent)
			{
				Marking->GetRoad()->GetScene()->Rebuild();
			});
	}
}

FVector FRoadTool_MarkingCurve::GetWidgetLocation() const
{
	if (CurrentMarking)
		return CurrentMarking->GetRoad()->GetPos(PointIndex != INDEX_NONE ? CurrentMarking->Points[PointIndex].GetUV(SubIndex) : CurrentMarking->Center());
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_MarkingCurve::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (CurrentMarking)
	{
		InMatrix = FRotationMatrix(CurrentMarking->GetRoad()->GetDir(PointIndex != INDEX_NONE ? CurrentMarking->Points[PointIndex].GetUV(SubIndex).X : CurrentMarking->Center().X).Rotation());
		return true;
	}
	return false;
}

bool FRoadTool_MarkingCurve::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HandleClickRoad(HitProxy, Click))
		return true;
	ARoadActor* SelectedRoad = GetSelectedRoad();
	auto ClickProxy = [&](HRoadMarkingProxy* Proxy)
	{
		CurrentMarking = Cast<UMarkingCurve>(Proxy->Marking);
		PointIndex = Proxy->Index;
		SubIndex = Proxy->SubId;
		ShowMarking(CurrentMarking, PointIndex);
	};
	if (Click.GetKey() == EKeys::LeftMouseButton)
	{
		if (HRoadMarkingProxy* Proxy = HitProxyCast<HRoadMarkingProxy>(HitProxy))
			ClickProxy(Proxy);
		else
			Reset();
		return true;
	}
	if (Click.GetKey() == EKeys::RightMouseButton)
	{
		const FScopedTransaction Transaction(LOCTEXT("MarkingCurve", "MarkingCurve"));
		HRoadMarkingProxy* Proxy = HitProxyCast<HRoadMarkingProxy>(HitProxy);
		if (CurrentMarking && CurrentMarking->IsEndPoint(PointIndex))
		{
			if (Proxy)
			{
				if (Proxy->Marking == CurrentMarking && Proxy->Index != PointIndex && CurrentMarking->IsEndPoint(Proxy->Index))
					CurrentMarking->MakeClose();
				else
					ClickProxy(Proxy);
			}
			else
				CurrentMarking->InsertPoint(SelectedRoad->GetUV(LineTrace(InViewportClient)), PointIndex);
		}
		else if (Proxy)
			ClickProxy(Proxy);
		else if (SelectedRoad)
		{
			SelectedRoad->Modify();
			CurrentMarking = SelectedRoad->AddMarkingCurve();
			PointIndex = INDEX_NONE;
			CurrentMarking->InsertPoint(SelectedRoad->GetUV(LineTrace(InViewportClient)), PointIndex);
			ShowMarking(CurrentMarking, PointIndex);
		}
		return true;
	}
	return false;
}

bool FRoadTool_MarkingCurve::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (FRoadTool::InputKey(ViewportClient, Viewport, Key, Event))
		return true;
	if (Event == IE_Pressed && CurrentMarking)
	{
		if (Key == EKeys::Delete)
		{
			ARoadActor* SelectedRoad = GetSelectedRoad();
			SelectedRoad->DeleteMarking(CurrentMarking);
			// Reset() clears CurrentMarking/PointIndex/SubIndex and empties the inspector; without it the panel keeps the deleted object.
			Reset();
			SelectedRoad->GetScene()->Rebuild();
			return true;
		}
	}
	return false;
}

bool FRoadTool_MarkingCurve::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		if (CurrentMarking)
		{
			FMatrix Mat = GLevelEditorModeTools().GetCustomDrawingCoordinateSystem();
			FVector LocalDrag = Mat.InverseTransformVector(InDrag);
			CurrentMarking->Modify();
			if (PointIndex != INDEX_NONE)
				CurrentMarking->Points[PointIndex].ApplyDelta(SubIndex, (FVector2D&)LocalDrag);
			else
			{
				for (FMarkingCurvePoint& Point : CurrentMarking->Points)
					Point.Pos += (FVector2D&)LocalDrag;
			}
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_MarkingCurve::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	auto Draw = [&](ARoadActor* Road)
	{
		for (URoadMarking* Marking : Road->Markings)
		{
			if (UMarkingCurve* MarkingCurve = Cast<UMarkingCurve>(Marking))
			{
				PDI->SetHitProxy(new HRoadMarkingProxy(Marking, INDEX_NONE));
				DrawCurve(PDI, MarkingCurve->CreatePolyline(), CurrentMarking == Marking ? Color_Select : Color_Line, Thickness_Road);
				if (CurrentMarking == MarkingCurve)
				{
					for (int i = 0; i < MarkingCurve->Points.Num(); i++)
					{
						for (int j = 0; j < (i == PointIndex ? 3 : 1); j++)
						{
							FVector Pos = MarkingCurve->GetRoad()->GetPos(MarkingCurve->Points[i].GetUV(j));
							PDI->SetHitProxy(new HRoadMarkingProxy(Marking, i, j));
							PDI->DrawPoint(Pos, i == PointIndex ? Color_Select : Color_Line, Size_Point, SDPG_Foreground);
							if (j > 0)
								PDI->DrawLine(Pos, MarkingCurve->GetRoad()->GetPos(MarkingCurve->Points[i].GetUV(0)), Color_Select, SDPG_Foreground, Thickness_Line);
						}
					}
				}
			}
		}
	};
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
		Draw(SelectedRoad);
	else
		DrawRoads(PDI, true);
}

#undef LOCTEXT_NAMESPACE
