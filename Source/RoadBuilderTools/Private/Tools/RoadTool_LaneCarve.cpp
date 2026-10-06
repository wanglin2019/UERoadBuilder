// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_LaneCarve.h"

#include "RoadActor.h"
#include "RoadBoundary.h"
#include "RoadLane.h"
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

void URoadTool_LaneCarve::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;

	if (!bRightButton)
	{
		// A left click only selects; it deliberately does not disarm an armed carve, because the legacy
		// tool did not either - so the user can look at another road and come back.
		SelectRoadUnderRay(Ray);
		return;
	}

	CarveAtRay(Ray);
}

void URoadTool_LaneCarve::CarveAtRay(const FRay& Ray)
{
	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return;
	}

	URoadBoundary* Boundary = RoadPicking::PickBoundary(Road, Ray);
	if (Boundary == nullptr)
	{
		return;
	}

	// Station comes from the world hit, the lateral offset from the boundary that was hit - which is what
	// makes the carve follow the boundary rather than the exact cursor position.
	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		// The legacy tool projected the sentinel, carving at an arbitrary station. Dropping the click is
		// the only defensible reading of a trace that hit nothing.
		return;
	}

	FVector2D EndUV = Road->GetUV(Position);
	EndUV.Y = Boundary->GetOffset(EndUV.X);

	if (StartUV.X < 0.0)
	{
		StartUV = EndUV;
		RequestRedraw();
		return;
	}

	ApplyCarve(Road, EndUV);

	// Disarm either way: a failed carve has to leave a clean slate too, or the next click would pair with
	// a stale start.
	ResetCarve();
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

	// CopyLane() creates a lane object, so the edit is bracketed by a host transaction. The Modify() calls
	// are kept: they are what records the lane and boundary sub-objects, not just the road, with the
	// host's undo stack.
	FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("LaneCarve", "Lane Carve"));
	Road->Modify();

	URoadLane* ForkLane = nullptr;
	if (URoadLane* SideLane = Lane->GetBoundary(Side)->GetLane(Side))
	{
		// A neighbour lane already zero-width at both ends is the fork that would be created anyway.
		if (FMath::IsNearlyZero(SideLane->GetWidth(StartStation))
			&& FMath::IsNearlyZero(SideLane->GetWidth(EndStation)))
		{
			ForkLane = SideLane;
		}
	}
	if (ForkLane == nullptr)
	{
		ForkLane = Road->CopyLane(Lane, Side);
	}

	// The source lane keeps its width and only opens at the far end...
	{
		Lane->Modify();
		URoadBoundary* Boundary = Lane->GetBoundary(LaneSide);
		Boundary->Modify();
		// The start-side index is unused at this end - only the insert as a side effect matters.
		Boundary->AddLocalOffset(StartStation);
		const int32 NextIndex = Boundary->AddLocalOffset(EndStation);
		Boundary->LocalOffsets[NextIndex].Offset = 0.0;
		Boundary->LocalOffsets[NextIndex + 1].Offset = 0.0;
		Boundary->AddSegment(StartStation);
		Boundary->AddSegment(EndStation);
	}

	// ...and the fork lane does the opposite: zero width at first, full width at the far end.
	{
		ForkLane->Modify();
		URoadBoundary* Boundary = ForkLane->GetBoundary(LaneSide);
		Boundary->Modify();
		const int32 PrevIndex = Boundary->AddLocalOffset(StartStation);
		const int32 NextIndex = Boundary->AddLocalOffset(EndStation);
		Boundary->LocalOffsets[PrevIndex - 1].Offset = 0.0;
		Boundary->LocalOffsets[PrevIndex].Offset = 0.0;
		Boundary->LocalOffsets[NextIndex].Offset = LaneWidth;
		Boundary->LocalOffsets[NextIndex + 1].Offset = LaneWidth;
		Boundary->AddSegment(StartStation);
		Boundary->AddSegment(EndStation);
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
