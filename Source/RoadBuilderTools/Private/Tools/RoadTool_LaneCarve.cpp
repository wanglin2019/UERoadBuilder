// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_LaneCarve.h"

#include "RoadActor.h"
#include "RoadBoundary.h"
#include "RoadLane.h"
#include "RoadLog.h"
#include "SceneManagement.h"
#include "Tools/RoadPicking.h"

#define LOCTEXT_NAMESPACE "RoadTool_LaneCarve"

UInteractiveTool* URoadTool_LaneCarveBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_LaneCarve* NewTool = NewObject<URoadTool_LaneCarve>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_LaneCarve::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// Hover, because the armed carve is drawn out to the cursor: the render pass has no cursor ray of its
	// own, so it is captured through the input path and read back in Render().
	AddHoverBehavior();
}

void URoadTool_LaneCarve::Render(IToolsContextRenderAPI* RenderAPI)
{
	UInteractiveTool::Render(RenderAPI);

	FPrimitiveDrawInterface* PDI = (RenderAPI != nullptr) ? RenderAPI->GetPrimitiveDrawInterface() : nullptr;
	if (PDI == nullptr)
	{
		return;
	}

	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr || Road->Length() <= 0.0)
	{
		// Nothing selected yet, so offer the roads to select from.
		DrawRoads(PDI);
		return;
	}

	// A road is selected, so its boundaries are the carve's targets: draw those and nothing else.
	for (URoadBoundary* Boundary : Road->Boundaries)
	{
		if (Boundary != nullptr)
		{
			DrawCurve(PDI, Boundary->Curve, RoadToolStyle::Color_Line, RoadToolStyle::Thickness_Line);
		}
	}

	if (StartUV.X < 0.0)
	{
		return;
	}

	const FVector StartPosition = Road->GetPos(StartUV);
	PDI->DrawPoint(StartPosition, RoadToolStyle::Color_Line, RoadToolStyle::Size_Point, SDPG_Foreground);

	FRay CursorRay;
	if (!GetLastCursorRay(CursorRay))
	{
		return;
	}
	const FVector CursorPosition = LineTrace(CursorRay);
	if (CursorPosition.X >= WORLD_MAX)
	{
		return;
	}

	const FVector EndPosition = Road->GetPos(Road->GetUV(CursorPosition));
	PDI->DrawLine(StartPosition, EndPosition, RoadToolStyle::Color_Line, SDPG_Foreground,
		RoadToolStyle::Thickness_Line, 0.0f, true);
}

bool URoadTool_LaneCarve::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;

	if (!bRightButton)
	{
		// A left click only selects; it deliberately does not disarm an armed carve, because the legacy
		// tool did not either - so the user can look at another road and come back.
		SelectRoadUnderRay(Ray);
		return true;
	}

	return CarveAtRay(Ray);
}

bool URoadTool_LaneCarve::CarveAtRay(const FRay& Ray)
{
	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return false;
	}

	URoadBoundary* Boundary = RoadPicking::PickBoundary(Road, Ray);
	if (Boundary == nullptr)
	{
		return false;
	}

	// Station comes from the world hit, the lateral offset from the boundary that was hit - which is what
	// makes the carve follow the boundary rather than the exact cursor position.
	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		// The legacy tool projected the sentinel, carving at an arbitrary station. Dropping the click is
		// the only defensible reading of a trace that hit nothing.
		return false;
	}

	FVector2D EndUV = Road->GetUV(Position);
	EndUV.Y = Boundary->GetOffset(EndUV.X);

	if (StartUV.X < 0.0)
	{
		StartUV = EndUV;
		RequestRedraw();
		return true;
	}

	ApplyCarve(Road, EndUV);

	// Disarm either way: a failed carve has to leave a clean slate too, or the next click would pair with
	// a stale start. The click was aimed at a boundary either way, so it stays consumed even when the
	// carve itself found nothing to change.
	ResetCarve();
	return true;
}

bool URoadTool_LaneCarve::ApplyCarve(ARoadActor* Road, const FVector2D& EndUV)
{
	if (Road == nullptr)
	{
		return false;
	}

	// These carry stations (UV.X) at each end of the carve. The legacy names are kept even though they say
	// "Offset", because the lateral offsets are now looked up from the boundaries instead.
	double StartStation = StartUV.X;
	double EndStation = EndUV.X;

	TArray<URoadBoundary*> StartBoundaries = Road->GetBoundaries(StartUV);
	TArray<URoadBoundary*> EndBoundaries = Road->GetBoundaries(EndUV);
	if (StartBoundaries.Num() == 0 || EndBoundaries.Num() == 0)
	{
		return false;
	}

	// Which lane both ends share. Either end may be the one further along the road, so both orders are
	// tested - and the boundaries travel with their stations when the pair is swapped.
	if (EndStation < StartStation)
	{
		Swap(StartBoundaries, EndBoundaries);
		Swap(StartStation, EndStation);
	}

	URoadLane* Lane = nullptr;
	int32 Side = INDEX_NONE;
	if (StartBoundaries[0]->LeftLane == EndBoundaries.Last()->RightLane)
	{
		Lane = StartBoundaries[0]->LeftLane;
		Side = 0;
	}
	else if (StartBoundaries.Last()->RightLane == EndBoundaries[0]->LeftLane)
	{
		Lane = EndBoundaries[0]->LeftLane;
		Side = 1;
	}

	if (Lane == nullptr)
	{
		// The two clicks did not span a single lane, so there is nothing to carve. The legacy code tested
		// a sentinel here; a null lane is the same condition.
		return false;
	}

	const int32 LaneSide = Lane->GetSide();
	const double LaneWidth = Lane->GetWidth((StartStation + EndStation) / 2);

	// The offset writes below index one past and one before the freshly inserted points. Those indexes
	// are safe by construction as long as the boundary carries at least two offsets: GetPointIndex()
	// clamps its answer to [0, Num-2], so AddLocalOffset() inserts at [1, Num-1] and the neighbours the
	// code touches always exist. A boundary with fewer than two offsets breaks that contract (and would
	// fault inside AddLocalOffset itself), so it is rejected up front rather than traced through.
	URoadBoundary* SourceBoundary = Lane->GetBoundary(LaneSide);
	if (SourceBoundary == nullptr || SourceBoundary->LocalOffsets.Num() < 2)
	{
		RoadLog_Warn(TEXT("LaneCarve rejected: %s boundary has %d offsets, needs 2"),
			*Lane->GetName(),
			SourceBoundary != nullptr ? SourceBoundary->LocalOffsets.Num() : -1);
		return false;
	}

	// CopyLane() creates a lane object, so the edit is bracketed by a host transaction. The Modify() calls
	// are kept: they are what records the lane and boundary sub-objects, not just the road, with the
	// host's undo stack.
	FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("LaneCarve", "Lane Carve"));
	Road->Modify();

	URoadLane* ForkLane = nullptr;
	if (URoadLane* SideLane = Lane->GetBoundary(Side)->GetLane(Side))
	{
		// A neighbour lane already zero-width at both ends is the fork that would be created anyway. It is
		// only reused when its boundary satisfies the same offset contract as the source - otherwise the
		// carve forks a fresh copy, which inherits the source's (already validated) offsets instead.
		URoadBoundary* SideBoundary = SideLane->GetBoundary(LaneSide);
		if (FMath::IsNearlyZero(SideLane->GetWidth(StartStation))
			&& FMath::IsNearlyZero(SideLane->GetWidth(EndStation))
			&& SideBoundary != nullptr && SideBoundary->LocalOffsets.Num() >= 2)
		{
			ForkLane = SideLane;
		}
	}
	if (ForkLane == nullptr)
	{
		ForkLane = Road->CopyLane(Lane, Side);
	}

	URoadBoundary* ForkBoundary = (ForkLane != nullptr) ? ForkLane->GetBoundary(LaneSide) : nullptr;
	if (ForkBoundary == nullptr)
	{
		RoadLog_Warn(TEXT("LaneCarve rejected: %s has no boundary on side %d"),
			(ForkLane != nullptr) ? *ForkLane->GetName() : TEXT("null"), LaneSide);
		return false;
	}

	// The source lane keeps its width and only opens at the far end...
	{
		Lane->Modify();
		SourceBoundary->Modify();
		// The start-side index is unused at this end - only the insert as a side effect matters.
		SourceBoundary->AddLocalOffset(StartStation);
		const int32 NextIndex = SourceBoundary->AddLocalOffset(EndStation);
		SourceBoundary->LocalOffsets[NextIndex].Offset = 0.0;
		SourceBoundary->LocalOffsets[NextIndex + 1].Offset = 0.0;
		SourceBoundary->AddSegment(StartStation);
		SourceBoundary->AddSegment(EndStation);
	}

	// ...and the fork lane does the opposite: zero width at first, full width at the far end.
	{
		ForkLane->Modify();
		ForkBoundary->Modify();
		const int32 PrevIndex = ForkBoundary->AddLocalOffset(StartStation);
		const int32 NextIndex = ForkBoundary->AddLocalOffset(EndStation);
		ForkBoundary->LocalOffsets[PrevIndex - 1].Offset = 0.0;
		ForkBoundary->LocalOffsets[PrevIndex].Offset = 0.0;
		ForkBoundary->LocalOffsets[NextIndex].Offset = LaneWidth;
		ForkBoundary->LocalOffsets[NextIndex + 1].Offset = LaneWidth;
		ForkBoundary->AddSegment(StartStation);
		ForkBoundary->AddSegment(EndStation);
	}

	Road->UpdateLanes();
	RequestRebuild();
	return true;
}

void URoadTool_LaneCarve::ResetCarve()
{
	StartUV = FVector2D(-1, 0);
	RequestRedraw();
}

#undef LOCTEXT_NAMESPACE
