// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_LaneEdit.h"

#include "InputCoreTypes.h"
#include "RoadBoundary.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "RoadLane.h"
#include "SceneManagement.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadKeyInputBehavior.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPointGizmo.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_LaneEdit"

UInteractiveTool* URoadTool_LaneEditBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_LaneEdit* NewTool = NewObject<URoadTool_LaneEdit>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_LaneEdit::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The drag handle binding, which is what makes the axis handle grabbable; it declines every press
	// that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	// The four navigation keys the legacy tool handled. Each gets its own behaviour because ITF's
	// keyboard behaviours carry one key each and only feed modifiers to other behaviours.
	HomeKeyBehavior = NewObject<URoadKeyInputBehavior>();
	HomeKeyBehavior->Initialize(EKeys::Home, [this]() { MoveToNeighbourLane(-1); });
	AddInputBehavior(HomeKeyBehavior);

	EndKeyBehavior = NewObject<URoadKeyInputBehavior>();
	EndKeyBehavior->Initialize(EKeys::End, [this]() { MoveToNeighbourLane(1); });
	AddInputBehavior(EndKeyBehavior);

	TabKeyBehavior = NewObject<URoadKeyInputBehavior>();
	TabKeyBehavior->Initialize(EKeys::Tab, [this]() { SelectNextSegment(); });
	AddInputBehavior(TabKeyBehavior);

	DeleteKeyBehavior = NewObject<URoadKeyInputBehavior>();
	DeleteKeyBehavior->Initialize(EKeys::Delete, [this]() { DeleteSelectedSegment(); });
	AddInputBehavior(DeleteKeyBehavior);

	Properties = NewObject<URoadTool_LaneEditProperties>(this, TEXT("LaneEditSettings"));
	AddToolPropertySource(Properties);

	// A lane is dragged along the road only, so the gizmo exposes one axis and is rotated onto the tangent
	// - which is what the legacy GetCustomDrawingCoordinateSystem() achieved for the editor's own widget.
	Gizmo = NewObject<URoadPointGizmo>(this);
	Gizmo->Initialize(this, ETransformGizmoSubElements::TranslateAxisX,
		[this]() { return GetSegmentTransform(); },
		[this](const FTransform& NewTransform) { ApplySegmentTransform(NewTransform); },
		[this]() { RebuildAfterDrag(); });
}

void URoadTool_LaneEdit::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

URoadLane* URoadTool_LaneEdit::GetCurrentLane() const
{
	URoadLane* Lane = CurrentLane.Get();
	const ARoadActor* Road = GetSelectedRoad();
	if (Lane == nullptr || Road == nullptr)
	{
		return nullptr;
	}

	// A rebuild or an undo can replace the road's lanes wholesale, in which case the remembered lane is no
	// longer one of its own; the selection is stale and reads as empty.
	return Road->Lanes.Contains(Lane) ? Lane : nullptr;
}

void URoadTool_LaneEdit::SelectSegment(URoadLane* Lane, int32 InSegmentIndex)
{
	const bool bValid = (Lane != nullptr && Lane->Segments.IsValidIndex(InSegmentIndex));
	CurrentLane = bValid ? Lane : nullptr;
	SegmentIndex = bValid ? InSegmentIndex : INDEX_NONE;

	SyncPropertiesFromSegment();
	Gizmo->Update(SegmentIndex != INDEX_NONE);

	// Only the highlight changed, so a repaint is enough - the legacy tool's Reset() invalidated the view
	// for the same reason and ran no rebuild.
	RequestRedraw();
}

void URoadTool_LaneEdit::SyncPropertiesFromSegment()
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || SegmentIndex == INDEX_NONE)
	{
		Properties->SegmentIndex = INDEX_NONE;
		Properties->Dist = 0.0;
		Properties->LaneShape = nullptr;
		Properties->LaneType = ELaneType::None;
		return;
	}

	const FLaneSegment& Segment = Lane->Segments[SegmentIndex];
	Properties->SegmentIndex = SegmentIndex;
	Properties->Dist = Segment.Dist;
	Properties->LaneShape = Segment.LaneShape;
	Properties->LaneType = Segment.LaneType;
}

FTransform URoadTool_LaneEdit::GetSegmentTransform() const
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || SegmentIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	const double Dist = Lane->SegmentStart(SegmentIndex);
	return FTransform(Lane->GetDir(Dist).Rotation(), Lane->GetPos(Dist));
}

void URoadTool_LaneEdit::ApplySegmentTransform(const FTransform& NewTransform)
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || SegmentIndex == INDEX_NONE)
	{
		return;
	}

	ARoadActor* Road = Lane->GetRoad();
	if (Road == nullptr)
	{
		return;
	}

	// The gizmo reports an absolute transform, but the data is a station, so what gets applied is the
	// difference between where the gizmo wants the point and where the point is. Measuring against the
	// data - never against the previous event - is what keeps the snapping below from dragging the gizmo
	// away from the segment it is editing, and it makes re-seating the gizmo a no-op.
	const double CurrentDist = Lane->SegmentStart(SegmentIndex);
	const FVector Delta = NewTransform.GetLocation() - Lane->GetPos(CurrentDist);
	if (Delta.IsNearlyZero())
	{
		return;
	}

	// The legacy tool decomposed its drag with the custom coordinate system it had set to the road
	// tangent, then moved the segment along X only.
	const FVector2D LocalDelta = ToRoadFrame(Road, CurrentDist, Delta);

	Lane->SegmentStart(SegmentIndex) = CurrentDist + LocalDelta.X;

	// SnapSegment() clamps to the neighbouring segments first, so a drag past a neighbour lands on it
	// rather than corrupting the sorted array the binary searches rely on.
	Lane->SnapSegment(SegmentIndex);

	// No explicit undo entry: the gizmo's own change replays this edit by calling back into the proxy,
	// which calls this function again with the previous transform.
	Road->UpdateLanes();
	SyncPropertiesFromSegment();

	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_LaneEdit::RebuildAfterDrag()
{
	RequestRebuild();
}

FInputRayHit URoadTool_LaneEdit::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Nothing to grab without a selected segment, and nothing to measure against without a scale. The
	// scale is the gizmo's own: it is measured in Render() every frame, the frame the user aimed at.
	URoadLane* Lane = GetCurrentLane();
	const double PixelToWorld = (Gizmo != nullptr) ? Gizmo->GetLastPixelToWorld() : 0.0;
	if (Gizmo == nullptr || Lane == nullptr || SegmentIndex == INDEX_NONE || PixelToWorld <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, PixelToWorld);

	// The handle test's own verdict, per tool, so a press that fails to start a drag says whether it found
	// no handle or was overruled downstream. Verbose: one line per press, not per move.
	RoadLog_Debug(TEXT("drag laneedit handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes in ApplySegmentTransform record nothing themselves.
	DragChange = FRoadArrayChange::CaptureBefore(Lane, GetLaneSegmentsProperty());

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit; the depth keeps this
	// behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_LaneEdit::OnRoadDragged(const FInputDeviceRay& DragPos)
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

void URoadTool_LaneEdit::OnRoadDragEnded()
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
		if (URoadLane* Lane = GetCurrentLane())
		{
			DragChange->CaptureAfter();
			EmitArrayChange(Lane, MoveTemp(DragChange), LOCTEXT("MoveLaneSegment", "Move Lane Segment"));
		}
		DragChange.Reset();
	}
}

void URoadTool_LaneEdit::ApplyPropertiesToSegment(FProperty* Property)
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || SegmentIndex == INDEX_NONE || Property == nullptr)
	{
		return;
	}

	ARoadActor* Road = Lane->GetRoad();
	if (Road == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Lane, GetLaneSegmentsProperty());
	if (Change == nullptr)
	{
		return;
	}

	FLaneSegment& Segment = Lane->Segments[SegmentIndex];
	const FName ChangedName = Property->GetFName();

	// The legacy tool reacted to two fields specifically and let the rest through untouched, so the field
	// name - not a blanket field copy - is what drives this.
	if (ChangedName == GET_MEMBER_NAME_CHECKED(FLaneSegment, Dist))
	{
		Segment.Dist = Properties->Dist;
		ClampDist(Lane->Segments, SegmentIndex, Road->Length());
	}
	else if (ChangedName == GET_MEMBER_NAME_CHECKED(FLaneSegment, LaneType))
	{
		Segment.LaneType = Properties->LaneType;
		if (Segment.LaneType == ELaneType::Collapsed)
		{
			// A collapsed lane has no width, so the boundary on the lane's own side is pinned onto the road.
			URoadBoundary* Boundary = Lane->GetSide() ? Lane->LeftBoundary : Lane->RightBoundary;
			if (Boundary != nullptr)
			{
				Boundary->SetZeroOffset(Segment.Dist, Lane->SegmentEnd(SegmentIndex));
			}
		}
	}
	else if (ChangedName == GET_MEMBER_NAME_CHECKED(FLaneSegment, LaneShape))
	{
		Segment.LaneShape = Properties->LaneShape;
	}

	Change->CaptureAfter();
	EmitArrayChange(Lane, MoveTemp(Change), LOCTEXT("EditLaneSegment", "Edit Lane Segment"));

	Road->UpdateLanes();
	SyncPropertiesFromSegment();
	RequestRebuild();
}

void URoadTool_LaneEdit::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	// SegmentIndex is the tool's own bookkeeping, not something the user edits.
	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URoadTool_LaneEditProperties, SegmentIndex))
	{
		return;
	}

	ApplyPropertiesToSegment(Property);
}

void URoadTool_LaneEdit::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;
	ARoadActor* Road = GetSelectedRoad();

	// With nothing selected the roads are what is on screen, so the first click picks one. This is the
	// legacy HandleClickRoad(), and like it, it only answers the left button.
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

	// A road is selected, so the cursor's station and offset on it are what identify the target.
	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		// The legacy tool projected this sentinel and landed at an arbitrary station; dropping the click is
		// the only defensible reading of a trace that hit nothing.
		return;
	}
	const FVector2D UV = Road->GetUV(Position);

	if (!bRightButton)
	{
		URoadLane* Lane = Road->GetLane(UV);
		const int32 Index = (Lane != nullptr && Lane->Segments.Num() > 0) ? Lane->GetSegment(UV.X) : INDEX_NONE;
		SelectSegment(Lane, Index);
		return;
	}

	URoadLane* SelectedLane = GetCurrentLane();
	if (SelectedLane == nullptr)
	{
		return;
	}

	// A right click on one of the current lane's two boundaries copies the lane across it. Only those two
	// are offered to the ray, which is exactly the set the legacy Render() drew with hit proxies.
	TArray<URoadBoundary*> Boundaries;
	Boundaries.Add(SelectedLane->LeftBoundary);
	Boundaries.Add(SelectedLane->RightBoundary);

	int32 BoundaryIndex = INDEX_NONE;
	if (RoadPicking::PickBoundary(Boundaries, Ray, BoundaryIndex) != nullptr)
	{
		CopyLaneAcrossBoundary(Road, /*bLeftBoundary*/ BoundaryIndex == 0);
		return;
	}

	// Otherwise it acts on the lane itself: the same lane gets a new segment at the cursor, a different
	// one just becomes the selection.
	URoadLane* Lane = Road->GetLane(UV);
	if (Lane == nullptr)
	{
		return;
	}

	if (Lane == SelectedLane)
	{
		AddSegmentAt(Road, UV.X);
		return;
	}

	const int32 Index = (Lane->Segments.Num() > 0) ? Lane->GetSegment(UV.X) : INDEX_NONE;
	SelectSegment(Lane, Index);
}

void URoadTool_LaneEdit::SelectParent()
{
	SelectSegment(nullptr, INDEX_NONE);
	Super::SelectParent();
}

void URoadTool_LaneEdit::AddSegmentAt(ARoadActor* Road, double Dist)
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || Road == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Lane, GetLaneSegmentsProperty());
	if (Change == nullptr)
	{
		return;
	}

	// AddSegment() answers with the index of the segment it inserted, so the new one becomes the selection.
	const int32 NewIndex = Lane->AddSegment(FMath::Clamp(Dist, 0.0, Road->Length()));
	Change->CaptureAfter();
	EmitArrayChange(Lane, MoveTemp(Change), LOCTEXT("AddLaneSegment", "Add Lane Segment"));

	Road->UpdateLanes();
	SelectSegment(Lane, NewIndex);
	RequestRebuild();
}

void URoadTool_LaneEdit::CopyLaneAcrossBoundary(ARoadActor* Road, bool bLeftBoundary)
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || Road == nullptr)
	{
		return;
	}

	// CopyLane() creates a lane object and re-parents the boundaries it splits, so this is not a change to
	// a single object and no FToolCommandChange can describe it. The host transaction can - which is the
	// one place the legacy tool reached for FScopedTransaction too. The Modify() calls are kept: they are
	// what records the lane and boundary sub-objects, not just the road, with the host's undo stack.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("LaneEdit", "Lane Edit"));
		Road->Modify();
		Road->CopyLane(Lane, bLeftBoundary ? 1 : 0);
		Road->UpdateLanes();
	}

	// The copy may have replaced or re-created the lane objects, so the remembered selection is dropped
	// rather than trusted.
	SelectSegment(nullptr, INDEX_NONE);
	RequestRebuild();
}

void URoadTool_LaneEdit::DeleteSelectedSegment()
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || SegmentIndex == INDEX_NONE)
	{
		return;
	}

	ARoadActor* Road = Lane->GetRoad();
	if (Road == nullptr || Lane->Segments.Num() <= 1)
	{
		// A lane with no segments has nothing to say about its width, so the last one is not removable.
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Lane, GetLaneSegmentsProperty());
	if (Change == nullptr)
	{
		return;
	}

	Lane->DeleteSegment(SegmentIndex);
	Change->CaptureAfter();
	EmitArrayChange(Lane, MoveTemp(Change), LOCTEXT("DeleteLaneSegment", "Delete Lane Segment"));

	Road->UpdateLanes();
	SelectSegment(nullptr, INDEX_NONE);
	RequestRebuild();
}

void URoadTool_LaneEdit::MoveToNeighbourLane(int32 Step)
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || SegmentIndex == INDEX_NONE)
	{
		return;
	}

	// The legacy walk: Home goes through the lane's left boundary to whatever lane is on its far side,
	// End does the same on the right. Either may be absent at the edge of the road, and then nothing moves.
	URoadBoundary* Boundary = (Step < 0) ? Lane->LeftBoundary : Lane->RightBoundary;
	if (Boundary == nullptr)
	{
		return;
	}

	URoadLane* Neighbour = (Step < 0) ? Boundary->LeftLane : Boundary->RightLane;
	if (Neighbour == nullptr || Neighbour->Segments.Num() == 0)
	{
		return;
	}

	// The neighbour has its own segmentation, so the index is clamped rather than carried over blindly.
	SelectSegment(Neighbour, FMath::Min(SegmentIndex, Neighbour->Segments.Num() - 1));
}

void URoadTool_LaneEdit::SelectNextSegment()
{
	URoadLane* Lane = GetCurrentLane();
	if (Lane == nullptr || SegmentIndex == INDEX_NONE || Lane->Segments.Num() == 0)
	{
		return;
	}

	SelectSegment(Lane, (SegmentIndex + 1) % Lane->Segments.Num());
}

FArrayProperty* URoadTool_LaneEdit::GetLaneSegmentsProperty()
{
	return FindFProperty<FArrayProperty>(URoadLane::StaticClass(),
		GET_MEMBER_NAME_CHECKED(URoadLane, Segments));
}

void URoadTool_LaneEdit::Render(IToolsContextRenderAPI* RenderAPI)
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

	URoadLane* Lane = GetCurrentLane();

	// Faithful to the legacy pass: the current lane's two boundaries are drawn with the lane loop below
	// rather than here, so that the selected segment can be highlighted on them.
	for (URoadBoundary* Boundary : Road->Boundaries)
	{
		if (Boundary == nullptr)
		{
			continue;
		}
		if (Lane != nullptr && (Boundary == Lane->LeftBoundary || Boundary == Lane->RightBoundary))
		{
			continue;
		}
		DrawCurve(PDI, Boundary->Curve, RoadToolStyle::Color_Line, RoadToolStyle::Thickness_Line);
	}

	for (URoadLane* OtherLane : Road->Lanes)
	{
		if (OtherLane == nullptr)
		{
			continue;
		}

		for (int32 Index = 0; Index < OtherLane->Segments.Num(); ++Index)
		{
			const double Start = OtherLane->SegmentStart(Index);
			const double End = OtherLane->SegmentEnd(Index);
			const bool bSelected = (OtherLane == Lane && Index == SegmentIndex);
			const FLinearColor BoundaryColor = bSelected ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Line;
			// The legacy highlight reaches one segment back as well, because a divider belongs to the
			// segment that starts at it and to the one that ends there.
			const bool bSpanSelected = (OtherLane == Lane && (Index == SegmentIndex || Index - 1 == SegmentIndex));
			const FLinearColor Color = bSpanSelected ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Line;

			if (OtherLane == Lane && OtherLane->RightBoundary != nullptr && OtherLane->LeftBoundary != nullptr)
			{
				DrawCurve(PDI, OtherLane->RightBoundary->CreatePolyline(Start, End), BoundaryColor,
					RoadToolStyle::Thickness_Line, RoadToolStyle::DepthBias_Select);
				DrawCurve(PDI, OtherLane->LeftBoundary->CreatePolyline(Start, End), BoundaryColor,
					RoadToolStyle::Thickness_Line, RoadToolStyle::DepthBias_Select);
			}

			DrawDivider(PDI, OtherLane, Start, Color);
			if (Index + 1 == OtherLane->Segments.Num())
			{
				DrawDivider(PDI, OtherLane, End, BoundaryColor);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
