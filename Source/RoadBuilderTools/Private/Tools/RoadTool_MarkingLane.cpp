// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_MarkingLane.h"

#include "InputCoreTypes.h"
#include "RoadBoundary.h"
#include "RoadBuilderTools.h"
#include "RoadLane.h"
#include "SceneManagement.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadKeyInputBehavior.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPointGizmo.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_MarkingLane"

UInteractiveTool* URoadTool_MarkingLaneBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_MarkingLane* NewTool = NewObject<URoadTool_MarkingLane>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_MarkingLane::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The drag handle binding, which is what makes the axis handle grabbable; it declines every press
	// that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	// The navigation keys the legacy tool handled, one behaviour each: ITF has no keyboard behaviour that
	// runs tool logic directly.
	HomeKeyBehavior = NewObject<URoadKeyInputBehavior>();
	HomeKeyBehavior->Initialize(EKeys::Home, [this]() { MoveToNeighbourBoundary(-1); });
	AddInputBehavior(HomeKeyBehavior);

	EndKeyBehavior = NewObject<URoadKeyInputBehavior>();
	EndKeyBehavior->Initialize(EKeys::End, [this]() { MoveToNeighbourBoundary(1); });
	AddInputBehavior(EndKeyBehavior);

	TabKeyBehavior = NewObject<URoadKeyInputBehavior>();
	TabKeyBehavior->Initialize(EKeys::Tab, [this]() { SelectNextSegment(); });
	AddInputBehavior(TabKeyBehavior);

	DeleteKeyBehavior = NewObject<URoadKeyInputBehavior>();
	DeleteKeyBehavior->Initialize(EKeys::Delete, [this]() { DeleteSelectedSegment(); });
	AddInputBehavior(DeleteKeyBehavior);

	Properties = NewObject<URoadTool_MarkingLaneProperties>(this, TEXT("MarkingLaneSettings"));
	AddToolPropertySource(Properties);

	// A marking segment is a stretch of a boundary, so it only ever moves along the road: one axis, rotated
	// onto the tangent, which is what the legacy GetCustomDrawingCoordinateSystem() restricted the widget to.
	Gizmo = NewObject<URoadPointGizmo>(this);
	Gizmo->Initialize(this, ETransformGizmoSubElements::TranslateAxisX,
		[this]() { return GetSegmentTransform(); },
		[this](const FTransform& NewTransform) { ApplySegmentTransform(NewTransform); },
		[this]() { RebuildAfterDrag(); });
}

void URoadTool_MarkingLane::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

URoadBoundary* URoadTool_MarkingLane::GetCurrentBoundary() const
{
	URoadBoundary* Boundary = CurrentBoundary.Get();
	const ARoadActor* Road = GetSelectedRoad();
	if (Boundary == nullptr || Road == nullptr)
	{
		return nullptr;
	}

	// A rebuild or an undo can replace the road's boundaries wholesale, so the remembered one is only
	// trusted while the road still lists it.
	return Road->Boundaries.Contains(Boundary) ? Boundary : nullptr;
}

void URoadTool_MarkingLane::SelectSegment(URoadBoundary* Boundary, int32 InSegmentIndex)
{
	const bool bValid = (Boundary != nullptr && Boundary->Segments.IsValidIndex(InSegmentIndex));
	CurrentBoundary = bValid ? Boundary : nullptr;
	SegmentIndex = bValid ? InSegmentIndex : INDEX_NONE;

	SyncPropertiesFromSegment();
	Gizmo->Update(SegmentIndex != INDEX_NONE);

	// Only the highlight changed, so a repaint is enough; the legacy Reset() invalidated the view for the
	// same reason and ran no rebuild.
	RequestRedraw();
}

void URoadTool_MarkingLane::SyncPropertiesFromSegment()
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	if (Boundary == nullptr || SegmentIndex == INDEX_NONE)
	{
		Properties->SegmentIndex = INDEX_NONE;
		Properties->Dist = 0.0;
		Properties->LaneMarking = nullptr;
		Properties->Props = nullptr;
		return;
	}

	const FBoundarySegment& Segment = Boundary->Segments[SegmentIndex];
	Properties->SegmentIndex = SegmentIndex;
	Properties->Dist = Segment.Dist;
	Properties->LaneMarking = Segment.LaneMarking;
	Properties->Props = Segment.Props;
}

FTransform URoadTool_MarkingLane::GetSegmentTransform() const
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	if (Boundary == nullptr || SegmentIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	const double Dist = Boundary->SegmentStart(SegmentIndex);
	return FTransform(Boundary->GetDir(Dist).Rotation(), Boundary->GetPos(Dist));
}

void URoadTool_MarkingLane::ApplySegmentTransform(const FTransform& NewTransform)
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	if (Boundary == nullptr || SegmentIndex == INDEX_NONE)
	{
		return;
	}

	ARoadActor* Road = Boundary->GetRoad();
	if (Road == nullptr)
	{
		return;
	}

	// The gizmo reports an absolute position while the data is a station, so the delta is measured against
	// the data. That makes re-seating the gizmo a no-op and keeps the clamping below from drifting it.
	const double CurrentDist = Boundary->SegmentStart(SegmentIndex);
	const FVector Delta = NewTransform.GetLocation() - Boundary->GetPos(CurrentDist);
	if (Delta.IsNearlyZero())
	{
		return;
	}

	// The legacy tool decomposed its drag with the custom coordinate system it had set to the road tangent,
	// then moved the segment along X only.
	const FVector2D LocalDelta = ToRoadFrame(Road, CurrentDist, Delta);

	Boundary->SegmentStart(SegmentIndex) = CurrentDist + LocalDelta.X;

	// SnapSegment() clamps to the neighbouring segments first, so a drag past a neighbour lands on it
	// rather than corrupting the sorted array the binary searches rely on.
	Boundary->SnapSegment(SegmentIndex);

	// No explicit undo entry: the gizmo's own change replays this edit through the proxy.
	Road->UpdateLanes();
	SyncPropertiesFromSegment();

	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_MarkingLane::RebuildAfterDrag()
{
	RequestRebuild();
}

FInputRayHit URoadTool_MarkingLane::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Nothing to grab without a selected segment, and nothing to measure against without a scale. The
	// scale is the gizmo's own: it is measured in Render() every frame, the frame the user aimed at.
	URoadBoundary* Boundary = GetCurrentBoundary();
	const double PixelToWorld = (Gizmo != nullptr) ? Gizmo->GetLastPixelToWorld() : 0.0;
	if (Gizmo == nullptr || Boundary == nullptr || SegmentIndex == INDEX_NONE || PixelToWorld <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, PixelToWorld);

	// The handle test's own verdict, per tool, so a press that fails to start a drag says whether it found
	// no handle or was overruled downstream. Verbose: one line per press, not per move.
	UE_LOG(LogRoadBuilder, Verbose, TEXT("drag markinglane handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes in ApplySegmentTransform record nothing themselves.
	DragChange = FRoadArrayChange::CaptureBefore(Boundary, GetBoundarySegmentsProperty());

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit; the depth keeps this
	// behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_MarkingLane::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	if (DraggedHandle == ERoadGizmoHandle::None || Gizmo == nullptr)
	{
		return;
	}

	// The gizmo measures the cursor's motion along the tangent it fixed at the press and calls back into
	// ApplySegmentTransform, so the existing write path - the snap and the deferred rebuild included - is
	// all reused.
	Gizmo->DragHandle(DraggedHandle, DragPos.WorldRay);
}

void URoadTool_MarkingLane::OnRoadDragEnded()
{
	if (DraggedHandle == ERoadGizmoHandle::None)
	{
		return;
	}

	DraggedHandle = ERoadGizmoHandle::None;

	// Ends the drag first, so the geometry rebuild the tool defers until now is already applied when the
	// "after" snapshot below is taken.
	if (Gizmo != nullptr)
	{
		Gizmo->EndDrag();
	}

	if (DragChange != nullptr)
	{
		if (URoadBoundary* Boundary = GetCurrentBoundary())
		{
			DragChange->CaptureAfter();
			EmitArrayChange(Boundary, MoveTemp(DragChange), LOCTEXT("MoveMarkingSegment", "Move Marking Segment"));
		}
		DragChange.Reset();
	}
}

void URoadTool_MarkingLane::ApplyPropertiesToSegment()
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	if (Boundary == nullptr || SegmentIndex == INDEX_NONE)
	{
		return;
	}

	ARoadActor* Road = Boundary->GetRoad();
	if (Road == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Boundary, GetBoundarySegmentsProperty());
	if (Change == nullptr)
	{
		return;
	}

	FBoundarySegment& Segment = Boundary->Segments[SegmentIndex];
	Segment.Dist = Properties->Dist;
	Segment.LaneMarking = Properties->LaneMarking;
	Segment.Props = Properties->Props;

	// The legacy tool clamped the station whenever it was the edited field; clamping unconditionally keeps
	// the array sorted no matter which field the panel touched.
	ClampDist(Boundary->Segments, SegmentIndex, Road->Length());

	Change->CaptureAfter();
	EmitArrayChange(Boundary, MoveTemp(Change), LOCTEXT("EditMarkingSegment", "Edit Marking Segment"));

	Road->UpdateLanes();
	SyncPropertiesFromSegment();
	RequestRebuild();
}

void URoadTool_MarkingLane::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	// SegmentIndex is the tool's own bookkeeping, not something the user edits.
	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingLaneProperties, SegmentIndex))
	{
		return;
	}

	ApplyPropertiesToSegment();
}

void URoadTool_MarkingLane::PickUnderRay(const FRay& Ray, URoadBoundary*& OutBoundary, int32& OutIndex) const
{
	OutBoundary = nullptr;
	OutIndex = INDEX_NONE;

	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return;
	}

	FRoadHitCollector Collector(Ray);
	for (URoadBoundary* Boundary : Road->Boundaries)
	{
		if (Boundary == nullptr)
		{
			continue;
		}

		for (int32 Index = 0; Index < Boundary->Segments.Num(); ++Index)
		{
			// The legacy tools skipped the collapsed segments except on the centreline, because a zero-width
			// stretch is not something the user can see or aim at.
			if (Boundary != Road->BaseCurve && Boundary->IsZeroOffset(Index))
			{
				continue;
			}

			const double Start = Boundary->SegmentStart(Index);
			const double End = Boundary->SegmentEnd(Index);
			if (Start >= End)
			{
				continue;
			}

			// The sub-polyline stands in for the drawn segment and carries its own index, which is what the
			// legacy per-segment hit proxy did.
			Collector.ConsiderPolyline(Boundary->CreatePolyline(Start, End), Boundary, Index);
		}
	}

	const FRoadRayHit Hit = Collector.Resolve();
	if (Hit.bHit)
	{
		OutBoundary = Cast<URoadBoundary>(Hit.Owner);
		OutIndex = Hit.Index;
	}
}

void URoadTool_MarkingLane::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;
	ARoadActor* Road = GetSelectedRoad();

	// With nothing selected the roads themselves are on screen, so the left button picks one - the legacy
	// HandleClickRoad(), which also only answered the left button.
	if (Road == nullptr)
	{
		if (!bRightButton)
		{
			SelectRoadUnderRay(Ray);
		}
		return;
	}

	if (Road->Length() <= 0.0)
	{
		return;
	}

	URoadBoundary* Boundary = nullptr;
	int32 Index = INDEX_NONE;
	PickUnderRay(Ray, Boundary, Index);

	if (!bRightButton)
	{
		// A miss clears the selection, which is how the legacy tool emptied its panel.
		SelectSegment(Boundary, Index);
		return;
	}

	if (Boundary == nullptr)
	{
		return;
	}

	// A right click splits a boundary with a new segment where the cursor meets the road, so the station
	// has to come from a real world hit; the legacy tool projected a sentinel here, landing arbitrarily.
	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		return;
	}

	AddSegmentAt(Boundary, Road->GetUV(Position).X);
}

void URoadTool_MarkingLane::SelectParent()
{
	SelectSegment(nullptr, INDEX_NONE);
	Super::SelectParent();
}

void URoadTool_MarkingLane::AddSegmentAt(URoadBoundary* Boundary, double Dist)
{
	if (Boundary == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Boundary, GetBoundarySegmentsProperty());
	if (Change == nullptr)
	{
		return;
	}

	// AddSegment() answers with the index of the segment it inserted, so the new one becomes the selection.
	const int32 NewIndex = Boundary->AddSegment(Dist);
	Change->CaptureAfter();
	EmitArrayChange(Boundary, MoveTemp(Change), LOCTEXT("AddMarkingSegment", "Add Marking Segment"));

	ARoadActor* Road = Boundary->GetRoad();
	if (Road != nullptr)
	{
		Road->UpdateLanes();
	}

	SelectSegment(Boundary, NewIndex);
	RequestRebuild();
}

void URoadTool_MarkingLane::DeleteSelectedSegment()
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	if (Boundary == nullptr || SegmentIndex == INDEX_NONE)
	{
		return;
	}

	ARoadActor* Road = Boundary->GetRoad();
	if (Road == nullptr || Boundary->Segments.Num() <= 1)
	{
		// A boundary with no segments has nothing to say about its marking, so the last one is not removable.
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Boundary, GetBoundarySegmentsProperty());
	if (Change == nullptr)
	{
		return;
	}

	Boundary->DeleteSegment(SegmentIndex);
	Change->CaptureAfter();
	EmitArrayChange(Boundary, MoveTemp(Change), LOCTEXT("DeleteMarkingSegment", "Delete Marking Segment"));

	Road->UpdateLanes();
	SelectSegment(nullptr, INDEX_NONE);
	RequestRebuild();
}

void URoadTool_MarkingLane::MoveToNeighbourBoundary(int32 Step)
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	if (Boundary == nullptr || SegmentIndex == INDEX_NONE)
	{
		return;
	}

	// The legacy walk: Home goes out through the left lane to the boundary on its far side, End does the
	// same on the right. Either lane or either boundary may be absent at the edge of the road, and then
	// nothing moves.
	URoadLane* Lane = (Step < 0) ? Boundary->LeftLane : Boundary->RightLane;
	if (Lane == nullptr)
	{
		return;
	}

	URoadBoundary* Neighbour = (Step < 0) ? Lane->LeftBoundary : Lane->RightBoundary;
	if (Neighbour == nullptr || Neighbour->Segments.Num() == 0)
	{
		return;
	}

	// The neighbour has its own segmentation, so the index is clamped rather than carried over blindly.
	SelectSegment(Neighbour, FMath::Min(SegmentIndex, Neighbour->Segments.Num() - 1));
}

void URoadTool_MarkingLane::SelectNextSegment()
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	if (Boundary == nullptr || SegmentIndex == INDEX_NONE || Boundary->Segments.Num() == 0)
	{
		return;
	}

	SelectSegment(Boundary, (SegmentIndex + 1) % Boundary->Segments.Num());
}

FArrayProperty* URoadTool_MarkingLane::GetBoundarySegmentsProperty()
{
	return FindFProperty<FArrayProperty>(URoadBoundary::StaticClass(),
		GET_MEMBER_NAME_CHECKED(URoadBoundary, Segments));
}

void URoadTool_MarkingLane::Render(IToolsContextRenderAPI* RenderAPI)
{
	UInteractiveTool::Render(RenderAPI);

	FPrimitiveDrawInterface* PDI = (RenderAPI != nullptr) ? RenderAPI->GetPrimitiveDrawInterface() : nullptr;
	if (PDI == nullptr)
	{
		return;
	}

	// The handles are drawn by the gizmo itself, in immediate mode, because its actor-based visuals are
	// never reached by this tool's scene pass. See URoadPointGizmo::Render for the full explanation.
	if (Gizmo != nullptr)
	{
		Gizmo->Render(PDI, RenderAPI->GetSceneView());
	}

	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		DrawRoads(PDI);
		return;
	}

	if (Road->Length() <= 0.0)
	{
		return;
	}

	URoadBoundary* Current = GetCurrentBoundary();

	// Faithful to the legacy pass, including its skip of zero-width stretches: a collapsed segment draws
	// nothing, so it cannot be aimed at either.
	for (URoadBoundary* Boundary : Road->Boundaries)
	{
		if (Boundary == nullptr)
		{
			continue;
		}

		for (int32 Index = 0; Index < Boundary->Segments.Num(); ++Index)
		{
			if (Boundary != Road->BaseCurve && Boundary->IsZeroOffset(Index))
			{
				continue;
			}

			const double Start = Boundary->SegmentStart(Index);
			const double End = Boundary->SegmentEnd(Index);
			if (Start >= End)
			{
				continue;
			}

			const bool bSelected = (Boundary == Current && Index == SegmentIndex);
			const FLinearColor Color = bSelected ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Line;
			const float DepthBias = bSelected ? RoadToolStyle::DepthBias_Select : 0.0f;

			DrawCurve(PDI, Boundary->CreatePolyline(Start, End), Color, RoadToolStyle::Thickness_Line, DepthBias);
			DrawPoint(PDI, Boundary, Start, Color);
			if (Index + 1 == Boundary->Segments.Num())
			{
				DrawPoint(PDI, Boundary, End, Color);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
