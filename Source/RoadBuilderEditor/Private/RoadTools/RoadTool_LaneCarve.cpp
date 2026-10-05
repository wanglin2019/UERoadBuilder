#include "RoadTools/RoadTool_LaneCarve.h"
#include "RoadEdMode.h"
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


bool FRoadTool_LaneCarve::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (HandleClickRoad(HitProxy, Click))
		return true;
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		if (Click.GetKey() == EKeys::RightMouseButton)
		{
			if (HRoadCurveProxy* Proxy = HitProxyCast<HRoadCurveProxy>(HitProxy))
			{
				FVector2D EndUV = SelectedRoad->GetUV(LineTrace(InViewportClient));
				EndUV.Y = Proxy->Curve->GetOffset(EndUV.X);
				if (StartUV.X < 0)
					StartUV = EndUV;
				else
				{
					double StartOffset = StartUV.X;
					double EndOffset = EndUV.X;
					TArray<URoadBoundary*> StartBs = SelectedRoad->GetBoundaries(StartUV);
					TArray<URoadBoundary*> EndBs = SelectedRoad->GetBoundaries(EndUV);
					URoadLane* Lane = nullptr;
					int Side = INDEX_NONE;
					if (EndOffset < StartOffset)
					{
						Swap(StartBs, EndBs);
						Swap(StartOffset, EndOffset);
					}
					if (StartBs[0]->LeftLane == EndBs.Last()->RightLane)
					{
						Lane = StartBs[0]->LeftLane;
						Side = 0;
					}
					else if (StartBs.Last()->RightLane == EndBs[0]->LeftLane)
					{
						Lane = EndBs[0]->LeftLane;
						Side = 1;
					}
					if (Side != -1)
					{
						int LaneSide = Lane->GetSide();
						double LaneWidth = Lane->GetWidth((StartOffset + EndOffset) / 2);
						const FScopedTransaction Transaction(LOCTEXT("LaneCarve", "LaneCarve"));
						SelectedRoad->Modify();
						URoadLane* ForkLane = nullptr;
						if (URoadLane* SideLane = Lane->GetBoundary(Side)->GetLane(Side))
							if (FMath::IsNearlyZero(SideLane->GetWidth(StartOffset)) && FMath::IsNearlyZero(SideLane->GetWidth(EndOffset)))
								ForkLane = SideLane;
						if (!ForkLane)
							ForkLane = SelectedRoad->CopyLane(Lane, Side);
						{
							Lane->Modify();
						//	int Seg = Lane->AddSegment(EndOffset);
						//	Lane->Segments[Seg].LaneType = ELaneType::None;
							URoadBoundary* B = Lane->GetBoundary(LaneSide);
							B->Modify();
							int PrevIdx = B->AddLocalOffset(StartOffset);
							int NextIdx = B->AddLocalOffset(EndOffset);
						//	B->LocalOffsets[PrevIdx - 1].Offset = LaneWidth;
						//	B->LocalOffsets[PrevIdx].Offset = LaneWidth;
							B->LocalOffsets[NextIdx].Offset = 0;
							B->LocalOffsets[NextIdx + 1].Offset = 0;
							B->AddSegment(StartOffset);
							B->AddSegment(EndOffset);
						}
						{
							ForkLane->Modify();
						//	int Seg = ForkLane->AddSegment(StartOffset);
						//	ForkLane->Segments[Seg - 1].LaneType = ELaneType::None;
							URoadBoundary* B = ForkLane->GetBoundary(LaneSide);
							B->Modify();
							int PrevIdx = B->AddLocalOffset(StartOffset);
							int NextIdx = B->AddLocalOffset(EndOffset);
							B->LocalOffsets[PrevIdx - 1].Offset = 0;
							B->LocalOffsets[PrevIdx].Offset = 0;
							B->LocalOffsets[NextIdx].Offset = LaneWidth;
							B->LocalOffsets[NextIdx + 1].Offset = LaneWidth;
							B->AddSegment(StartOffset);
							B->AddSegment(EndOffset);
						}
						SelectedRoad->UpdateLanes();
						GetScene()->Rebuild();
					}
					Reset();
				}
				return true;
			}
		}
	}
	return false;
}

void FRoadTool_LaneCarve::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (ARoadActor* SelectedRoad = GetSelectedRoad())
	{
		if (SelectedRoad->Length() > 0)
		{
			for (URoadBoundary* Boundary : SelectedRoad->Boundaries)
			{
				PDI->SetHitProxy(new HRoadCurveProxy(Boundary));
				DrawCurve(PDI, Boundary->Curve, Color_Line, Thickness_Line);
			}
			if (StartUV.X >= 0)
			{
				FVector StartPos = SelectedRoad->GetPos(StartUV);
				PDI->DrawPoint(StartPos, Color_Line, Size_Point, SDPG_Foreground);
				FVector Pos = LineTrace(static_cast<FEditorViewportClient*>(Viewport->GetClient()));
				if (Pos.X < WORLD_MAX)
				{
					FVector2D EndUV = SelectedRoad->GetUV(Pos);
					FVector EndPos = SelectedRoad->GetPos(EndUV);
					PDI->DrawLine(StartPos, EndPos, Color_Line, SDPG_Foreground, Thickness_Line, 0, true);
				}
			}
		}
	}
	else
		DrawRoads(PDI, false);
}