#include "RoadTools/RoadTool_GroundEdit.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "EditorModes.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Editor/TransBuffer.h"
#include "DynamicMeshBuilder.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

void FRoadTool_GroundEdit::ShowPoint(AGroundActor* Ground, int Index)
{
	FRoadInspector* Inspector = GetInspector();
	if (!Inspector)
		return;
	if (!Ground || Index == INDEX_NONE)
	{
		// Leave the object panel empty when no specific point is selected, so stale content from the previous tool is not shown
		Inspector->ShowObject(nullptr);
		return;
	}

	auto Rebuild = [Ground](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		Ground->GetScene()->Rebuild();
	};

	if (Ground->Points[Index].Road)
		Inspector->ShowStruct(FGroundPoint::StaticStruct(), &Ground->Points[Index], Rebuild);
	else
		Inspector->ShowStruct(FindObject<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector")), &Ground->ManualPoints[Ground->Points[Index].Index], Rebuild);
}

FVector FRoadTool_GroundEdit::GetWidgetLocation() const
{
	AGroundActor* Ground = GetSelectedGround();
	if (PointIndex != INDEX_NONE && Ground->Points[PointIndex].Road == nullptr)
		return Ground->ManualPoints[Ground->Points[PointIndex].Index];
	return FRoadTool::GetWidgetLocation();
}

bool FRoadTool_GroundEdit::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	ARoadScene* Scene = GetScene();
	AGroundActor*& SelectedGround = GetSelectedGround();
	if (Click.GetKey() == EKeys::LeftMouseButton)
	{
		if (HGroundProxy* Proxy = HitProxyCast<HGroundProxy>(HitProxy))
		{
			SelectedGround = Proxy->Ground;
			PointIndex = Proxy->Index;
			ShowPoint(SelectedGround, PointIndex);
		}
		return true;
	}
	if (Click.GetKey() == EKeys::RightMouseButton)
	{
		if (SelectedGround && SelectedGround->IsEndPoint(PointIndex))
		{
			const FScopedTransaction Transaction(LOCTEXT("GroundEdit", "GroundEdit"));
			if (HGroundProxy* Proxy = HitProxyCast<HGroundProxy>(HitProxy))
			{
				if (Proxy->Ground != SelectedGround)
				{
					if (Proxy->Ground->IsEndPoint(Proxy->Index))
					{
						SelectedGround->Modify();
						SelectedGround->Join(Proxy->Ground, PointIndex);
						Scene->Grounds.Remove(Proxy->Ground);
						Proxy->Ground->Destroy();
						Scene->Rebuild();
						ShowPoint(SelectedGround, PointIndex);
					}
				}
				else if (Proxy->Index != PointIndex)
				{
					if (Proxy->Ground->IsEndPoint(Proxy->Index))
					{
						SelectedGround->Modify();
						SelectedGround->bClosedLoop = true;
						Scene->Rebuild();
						ShowPoint(SelectedGround, PointIndex);
					}
				}
			}
			else
			{
				FGroundPoint& Point = SelectedGround->Points[PointIndex];
				TMap<ARoadActor*, TArray<FJunctionSlot>> RoadSlots = Scene->GetAllJunctionSlots();
				FVector P = Point.Road ? Point.GetPos(RoadSlots) : SelectedGround->ManualPoints[Point.Index];
				FRay Ray = GetRay(InViewportClient);
				FVector Pos = FMath::RayPlaneIntersection(Ray.Origin, Ray.Direction, FPlane(FVector(0, 0, P.Z), FVector::UpVector));
				SelectedGround->Modify();
				SelectedGround->AddManualPoint(Pos, PointIndex);
				Scene->Rebuild();
				ShowPoint(SelectedGround, PointIndex);
			}
		}
		return true;
	}
	return false;
}

bool FRoadTool_GroundEdit::InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (InViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
	{
		AGroundActor* Ground = GetSelectedGround();
		if (PointIndex != INDEX_NONE && Ground->Points[PointIndex].Road == nullptr)
		{
			Ground->Modify();
			Ground->ManualPoints[Ground->Points[PointIndex].Index] += InDrag;
			LazyRebuild = true;
		}
		return true;
	}
	return false;
}

void FRoadTool_GroundEdit::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	ARoadScene* Scene = GetScene();
	TMap<ARoadActor*, TArray<FJunctionSlot>> RoadSlots = Scene->GetAllJunctionSlots();
	AGroundActor* SelectedGround = GetSelectedGround();
	for (AGroundActor* Ground : Scene->Grounds)
	{
		TArray<FVector> Vertices = Ground->GetVertices(RoadSlots);
		FColor Color = (Ground == SelectedGround) ? Color_Select : Color_Road;
		PDI->SetHitProxy(new HGroundProxy(Ground));
		for (int i = 0; i < Vertices.Num() - !Ground->bClosedLoop; i++)
		{
			const FVector& Start = Vertices[i];
			const FVector& End = Vertices[(i + 1) % Vertices.Num()];
			PDI->DrawLine(Start, End, Color, SDPG_Foreground, Thickness_Road, 0, true);
		}
		for (int i = 0; i < Ground->Points.Num(); i++)
		{
			FGroundPoint& Point = Ground->Points[i];
			PDI->SetHitProxy(new HGroundProxy(Ground, i));
			FVector Pos = Point.Road ? Point.GetPos(RoadSlots) : Ground->ManualPoints[Point.Index];
			PDI->DrawPoint(Pos, (Ground == SelectedGround && PointIndex == i) ? Color_Select : Color_Road, Size_Point, SDPG_Foreground);
		}
	}
}

#undef LOCTEXT_NAMESPACE
