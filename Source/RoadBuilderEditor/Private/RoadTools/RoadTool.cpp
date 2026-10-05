#include "RoadTools/RoadTool.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "Settings.h"
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

bool FRoadTool::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Event == IE_Pressed)
	{
		if (Key == EKeys::Escape)
		{
			SelectParent();
			return true;
		}
	}
	return false;
}

bool FRoadTool::EndModify()
{
	if (LazyRebuild)
	{
		GetScene()->Rebuild();
		LazyRebuild = false;
	}
	return true;
}

void FRoadTool::Reset()
{
	FEditorViewportClient* Client = GLevelEditorModeTools().GetFocusedViewportClient();
	Client->Invalidate();
	if (FRoadInspector* Inspector = GetInspector())
		Inspector->Clear();
}

ARoadScene* FRoadTool::GetScene() const
{
	return FEdModeRoad::Get()->Scene;
}

ARoadActor*& FRoadTool::GetSelectedRoad() const
{
	return FEdModeRoad::Get()->SelectedRoad;
}

AGroundActor*& FRoadTool::GetSelectedGround() const
{
	return FEdModeRoad::Get()->SelectedGround;
}

AJunctionActor*& FRoadTool::GetSelectedJunction() const
{
	return FEdModeRoad::Get()->SelectedJunction;
}

FRoadInspector* FRoadTool::GetInspector() const
{
	FEdModeRoad* Mode = FEdModeRoad::Get();
	return Mode ? Mode->GetInspector() : nullptr;
}

FRay FRoadTool::GetRay(FEditorViewportClient* ViewportClient) const
{
	FViewport* Viewport = ViewportClient->Viewport;
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, ViewportClient->GetScene(), ViewportClient->EngineShowFlags).SetRealtimeUpdate(ViewportClient->IsRealtime()));
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	FViewportCursorLocation MouseViewportRay(View, ViewportClient, Viewport->GetMouseX(), Viewport->GetMouseY());
	return FRay(MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection());
}

FVector FRoadTool::LineTrace(const FRay& Ray, AActor* IgnoredActor) const
{
	FHitResult Hit;
	FCollisionQueryParams Params;
	if (IgnoredActor)
		Params.AddIgnoredActor(IgnoredActor);
	if (GetScene()->GetWorld()->LineTraceSingleByChannel(Hit, Ray.Origin, Ray.Origin + Ray.Direction * 1000000.f, ECollisionChannel::ECC_Visibility, Params))
		return Hit.Location;
	return FVector(WORLD_MAX, WORLD_MAX, WORLD_MAX);
}

FVector FRoadTool::LineTrace(FEditorViewportClient* ViewportClient, AActor* IgnoredActor) const
{
	return LineTrace(GetRay(ViewportClient), IgnoredActor);
}

void FRoadTool::SelectParent()
{
	ARoadActor*& SelectedRoad = GetSelectedRoad();
	AJunctionActor*& SelectedJunction = GetSelectedJunction();
	if (SelectedRoad)
	{
		if (AJunctionActor* Junction = Cast<AJunctionActor>(SelectedRoad->GetAttachParentActor()))
			SelectedJunction = Junction;
		SelectedRoad = nullptr;
	}
	else if (SelectedJunction)
		SelectedJunction = nullptr;
	Reset();
}

bool FRoadTool::HandleClickRoad(HHitProxy* HitProxy, const FViewportClick& Click, int* PointIndex)
{
	if (Click.GetKey() == EKeys::LeftMouseButton)
	{
		ARoadActor*& SelectedRoad = GetSelectedRoad();
		if (HRoadProxy* Proxy = HitProxyCast<HRoadProxy>(HitProxy))
		{
			SelectedRoad = Proxy->Road;
			if (PointIndex)
				*PointIndex = Proxy->Index;
			return true;
		}
		/*
		if (!HitProxy || HitProxy->IsA(HActor::StaticGetType()))
		{
			Reset();
			return true;
		}*/
	}
	return false;
}

bool FRoadTool::HandleClickJunction(HHitProxy* HitProxy, const FViewportClick& Click, int* GateIndex, int* LinkIndex)
{
	if (Click.GetKey() == EKeys::LeftMouseButton)
	{
		AJunctionActor*& SelectedJunction = GetSelectedJunction();
		if (HJunctionProxy* Proxy = HitProxyCast<HJunctionProxy>(HitProxy))
		{
			SelectedJunction = Proxy->Junction;
			if (GateIndex)
				*GateIndex = Proxy->Index;
			if (LinkIndex)
				*LinkIndex = Proxy->SubId;
			return true;
		}
		/*
		if (!HitProxy || HitProxy->IsA(HActor::StaticGetType()))
		{
			Reset();
			return true;
		}*/
	}
	return false;
}

void FRoadTool::DrawCurve(FPrimitiveDrawInterface* PDI, const FPolyline& Curve, FColor Color, float Thickness, float DepthBias)
{
	for (int i = 0; i < Curve.Points.Num() - 1; i++)
	{
		const FVector& Start = Curve.Points[i].Pos;
		const FVector& End = Curve.Points[i + 1].Pos;
		PDI->DrawLine(Start, End, Color, SDPG_Foreground, Thickness, DepthBias, true);
	}
}

void FRoadTool::DrawPoint(FPrimitiveDrawInterface* PDI, URoadCurve* Curve, double Dist, FColor Color)
{
	FVector Point = Curve->GetPos(Dist);
	PDI->DrawPoint(Point, Color, Size_Point, SDPG_Foreground);
}

void FRoadTool::DrawDivider(FPrimitiveDrawInterface* PDI, URoadLane* Lane, double Dist, FColor Color)
{
	FVector Start = Lane->RightBoundary->GetPos(Dist);
	FVector End = Lane->LeftBoundary->GetPos(Dist);
	PDI->DrawLine(Start, End, Color, SDPG_Foreground);
}

void FRoadTool::DrawRoads(FPrimitiveDrawInterface* PDI, bool DrawLinks)
{
	ARoadActor* SelectedRoad = GetSelectedRoad();
	ARoadScene* Scene = GetScene();
	for (ARoadActor* Road : Scene->Roads)
	{
		PDI->SetHitProxy(new HRoadProxy(Road));
		FColor Color = Road == SelectedRoad ? Color_Select : Color_Road;
		if (Road->RoadPoints.Num() == 1)
			PDI->DrawPoint(Road->GetPos(0), Color, Size_Point, SDPG_Foreground);
		else
			DrawCurve(PDI, Road->BaseCurve->Curve, Color, Thickness_Road, Road == SelectedRoad ? DepthBias_Select : 0);
	}
	if (DrawLinks)
	{
		for (AJunctionActor* Junction : Scene->Junctions)
		{
			for (FJunctionGate& Gate : Junction->Gates)
			{
				for (int i = 0; i < Gate.Links.Num(); i++)
				{
					if (ARoadActor* Road = Gate.Links[i].Road)
					{
						if (URoadCurve* Curve = (i == 1) ? (URoadCurve*)Road->BaseCurve : (URoadCurve*)Road->BaseCurve->RightLane)
						{
							PDI->SetHitProxy(new HRoadProxy(Road));
							DrawCurve(PDI, Curve->Curve, (Road == SelectedRoad) ? Color_Select : Color_Road, Thickness_Road, Road == SelectedRoad ? DepthBias_Select : 0);
						}
					}
				}
			}
		}
	}
}

void FRoadTool::DrawJunction(FPrimitiveDrawInterface* PDI, AJunctionActor* Junction, FColor Color)
{
	TArray<FJunctionGate>& Gates = Junction->Gates;
	PDI->SetHitProxy(new HJunctionProxy(Junction));
	for (int i = 0; i < Gates.Num(); i++)
	{
		FJunctionGate& Gate = Gates[i];
		FJunctionGate& Next = Gates[(i + 1) % Gates.Num()];
		int SrcSide = Gate.Sign > 0 ? 0 : 1;
		int DstSide = Next.Sign > 0 ? 1 : 0;
		URoadBoundary* SrcBoundary = Gate.Road->GetRoadEdge(SrcSide);
		URoadBoundary* PrevBoundary = Gate.Road->GetRoadEdge(!SrcSide);
		URoadBoundary* DstBoundary = Next.Road->GetRoadEdge(DstSide);
		PDI->DrawLine(PrevBoundary->GetPos(Gate.Dist), SrcBoundary->GetPos(Gate.Dist), Color, SDPG_Foreground, Thickness_Road, 0, true);
		if (Gate.Links[1].Road)
			DrawCurve(PDI, Gate.Links[1].Road->BaseCurve->Curve, Color, Thickness_Road);
	}
	USettings_Global* Settings = GetMutableDefault<USettings_Global>();
	if (Settings->DisplayGateRadianPoints)
	{
#if 0
		FVector Center(0, 0, 0);
		for (FJunctionGate& Gate : Junction->Gates)
		{
			FVector Pos = Gate.Road->BaseCurve->GetPos(Gate.Dist);
			Center += Pos / Gates.Num();
		}
		PDI->DrawPoint(Center, FColor::Red, Size_Point, SDPG_Foreground);
		for (FJunctionGate& Gate : Junction->Gates)
		{
			FVector Pos = Gate.Road->BaseCurve->GetPos(Gate.Dist + Gate.Sign * DefaultJunctionExtent);
			PDI->DrawPoint(Pos, FColor::Blue, Size_Point, SDPG_Foreground);
		}
#elif 0
		for (FVector2D& Crossing : Junction->DebugCrossings)
		{
			//	FVector Pos = Gate.Road->BaseCurve->GetPos(Gate.Dist + Gate.Sign * DefaultJunctionExtent);
			PDI->DrawPoint(FVector(Crossing, 0), FColor::Blue, Size_Point, SDPG_Foreground);
		}
#elif 0
		for (int i = 0; i < Junction->DebugCurves.Num(); i++)
		{
			DrawCurve(PDI, Junction->DebugCurves[i], i % 2 ? FColor::Green : FColor::Red, Thickness_Road);
		}
#else
		PDI->SetHitProxy(nullptr);
		for (int i = 0; i < Junction->DebugPoints.Num(); i++)
		{
			FVector& Start = Junction->DebugPoints[i];
			FVector& End = Junction->DebugPoints[(i + 1) % Junction->DebugPoints.Num()];
			FVector Dir = (End - Start).GetSafeNormal();
			FVector N(-Dir.Y, Dir.X, Dir.Z);
			FVector Center = (Start + End) / 2;
			FVector Left = Center - N * 50 - Dir * 100;
			FVector Right = Center + N * 50 - Dir * 100;
			PDI->DrawLine(Start, End, FColor::Blue, SDPG_Foreground, Thickness_Line, 0, true);
			PDI->DrawLine(Left, Center, FColor::Blue, SDPG_Foreground, Thickness_Line, 0, true);
			PDI->DrawLine(Right, Center, FColor::Blue, SDPG_Foreground, Thickness_Line, 0, true);
		}
#endif
	}
}

void FRoadTool::DrawJunctions(FPrimitiveDrawInterface* PDI)
{
	ARoadScene* Scene = GetScene();
	AJunctionActor* SelectedJunction = GetSelectedJunction();
	for (AJunctionActor* Junction : Scene->Junctions)
		DrawJunction(PDI, Junction, (Junction == SelectedJunction) ? Color_Select : Color_Road);
}
