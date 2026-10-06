// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_LaneWidth.h"

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

#define LOCTEXT_NAMESPACE "RoadTool_LaneWidth"

UInteractiveTool* URoadTool_LaneWidthBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_LaneWidth* NewTool = NewObject<URoadTool_LaneWidth>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_LaneWidth::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The drag handle binding, which is what makes the translated handles grabbable; it declines every
	// press that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	DeleteKeyBehavior = NewObject<URoadKeyInputBehavior>();
	DeleteKeyBehavior->Initialize(EKeys::Delete, [this]() { DeleteSelectedOffset(); });
	AddInputBehavior(DeleteKeyBehavior);

	Properties = NewObject<URoadTool_LaneWidthProperties>(this, TEXT("LaneWidthSettings"));
	AddToolPropertySource(Properties);

	// The legacy tool asked for both X and Y in the road's frame, which is a translation plane once the
	// gizmo is rotated onto the boundary - X widens, Y slides the control point along the road.
	//
	// Plane + both axes: the rectangle self-culls whenever it is seen too close to edge-on, and here the
	// plane normal follows the road boundary rather than the world up axis, so it can go edge-on for an
	// arbitrary road heading. The axis arrows are the dependable half. See URoadTool_RoadPlan::Setup.
	Gizmo = NewObject<URoadPointGizmo>(this);
	Gizmo->Initialize(this,
		ETransformGizmoSubElements::TranslatePlaneXY |
		ETransformGizmoSubElements::TranslateAxisX |
		ETransformGizmoSubElements::TranslateAxisY,
		[this]() { return GetOffsetTransform(); },
		[this](const FTransform& NewTransform) { ApplyOffsetTransform(NewTransform); },
		[this]() { RebuildAfterDrag(); });
}

void URoadTool_LaneWidth::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

URoadBoundary* URoadTool_LaneWidth::GetCurrentBoundary() const
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

int32 URoadTool_LaneWidth::GetOffsetIndex() const
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	return (Boundary != nullptr && Boundary->LocalOffsets.IsValidIndex(OffsetIndex)) ? OffsetIndex : INDEX_NONE;
}

void URoadTool_LaneWidth::SelectOffset(URoadBoundary* Boundary, int32 Index)
{
	const bool bValid = (Boundary != nullptr && Boundary->LocalOffsets.IsValidIndex(Index));
	CurrentBoundary = bValid ? Boundary : nullptr;
	OffsetIndex = bValid ? Index : INDEX_NONE;

	SyncPropertiesFromOffset();
	Gizmo->Update(GetOffsetIndex() != INDEX_NONE);
	RequestRedraw();
}

void URoadTool_LaneWidth::SyncPropertiesFromOffset()
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	const int32 Index = GetOffsetIndex();
	if (Boundary == nullptr || Index == INDEX_NONE)
	{
		Properties->OffsetIndex = INDEX_NONE;
		Properties->Dist = 0.0;
		Properties->Offset = 0.0;
		Properties->Dir = 0.0;
		return;
	}

	const FCurveOffset& Element = Boundary->LocalOffsets[Index];
	Properties->OffsetIndex = Index;
	Properties->Dist = Element.Dist;
	Properties->Offset = Element.Offset;
	Properties->Dir = Element.Dir;
}

FTransform URoadTool_LaneWidth::GetOffsetTransform() const
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	const int32 Index = GetOffsetIndex();
	if (Boundary == nullptr || Index == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	const double Dist = Boundary->LocalOffsets[Index].Dist;
	return FTransform(Boundary->GetDir(Dist).Rotation(), Boundary->GetPos(Dist));
}

void URoadTool_LaneWidth::ApplyOffsetTransform(const FTransform& NewTransform)
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	const int32 Index = GetOffsetIndex();
	if (Boundary == nullptr || Index == INDEX_NONE)
	{
		return;
	}

	ARoadActor* Road = Boundary->GetRoad();
	if (Road == nullptr)
	{
		return;
	}

	// The gizmo reports where the point should be, not how far it moved, so the delta is measured against
	// the data. That makes re-seating the gizmo a no-op and stops the clamping below from drifting it.
	const double CurrentDist = Boundary->LocalOffsets[Index].Dist;
	const FVector Delta = NewTransform.GetLocation() - Boundary->GetPos(CurrentDist);
	if (Delta.IsNearlyZero())
	{
		return;
	}

	// The legacy tool decomposed its drag with the custom coordinate system it had set to the road
	// tangent: X moved the control point along the road, Y pushed the boundary out or in.
	const FVector2D LocalDelta = ToRoadFrame(Road, CurrentDist, Delta);

	FCurveOffset& Element = Boundary->LocalOffsets[Index];
	Element.Dist = CurrentDist + LocalDelta.X;
	Element.Offset += LocalDelta.Y;

	// SnapOffset() clamps the station to its neighbours first, so a fast drag cannot corrupt the sorted
	// array the binary searches rely on.
	Boundary->SnapOffset(Index);

	// No explicit undo entry: the gizmo's own change replays this edit through the proxy.
	Road->UpdateLanes();
	SyncPropertiesFromOffset();

	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_LaneWidth::RebuildAfterDrag()
{
	RequestRebuild();
}

FInputRayHit URoadTool_LaneWidth::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Nothing to grab without a selected control point, and nothing to measure against without a scale.
	// The scale is the gizmo's own: it is measured in Render() every frame, the frame the user aimed at.
	URoadBoundary* Boundary = GetCurrentBoundary();
	const int32 Index = GetOffsetIndex();
	const double PixelToWorld = (Gizmo != nullptr) ? Gizmo->GetLastPixelToWorld() : 0.0;
	if (Gizmo == nullptr || Boundary == nullptr || Index == INDEX_NONE || PixelToWorld <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, PixelToWorld);

	// The handle test's own verdict, per tool, so a press that fails to start a drag says whether it found
	// no handle or was overruled downstream. Verbose: one line per press, not per move.
	RoadLog_Debug(TEXT("drag lanewidth handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes in ApplyOffsetTransform record nothing themselves.
	DragChange = FRoadArrayChange::CaptureBefore(Boundary, GetLocalOffsetsProperty());

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit; the depth keeps this
	// behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_LaneWidth::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	if (DraggedHandle == ERoadGizmoHandle::None || Gizmo == nullptr)
	{
		return;
	}

	// The gizmo measures the cursor's motion in the subspace it fixed at the press and calls back into
	// ApplyOffsetTransform, so the existing write path - the road-frame decomposition, the snap and the
	// deferred rebuild - is all reused.
	Gizmo->DragHandle(DraggedHandle, DragPos.WorldRay);
}

void URoadTool_LaneWidth::OnRoadDragEnded()
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
			EmitArrayChange(Boundary, MoveTemp(DragChange), LOCTEXT("MoveWidthPoint", "Move Lane Width Point"));
		}
		DragChange.Reset();
	}
}

void URoadTool_LaneWidth::ApplyPropertiesToOffset()
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	const int32 Index = GetOffsetIndex();
	if (Boundary == nullptr || Index == INDEX_NONE)
	{
		return;
	}

	ARoadActor* Road = Boundary->GetRoad();
	if (Road == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Boundary, GetLocalOffsetsProperty());
	if (Change == nullptr)
	{
		return;
	}

	FCurveOffset& Element = Boundary->LocalOffsets[Index];
	Element.Dist = Properties->Dist;
	Element.Offset = Properties->Offset;
	Element.Dir = Properties->Dir;

	// The legacy tool clamped the station whenever it was the edited field; applying the same clamp here
	// keeps the array sorted no matter which field the panel touched.
	ClampDist(Boundary->LocalOffsets, Index, Road->Length());

	Change->CaptureAfter();
	EmitArrayChange(Boundary, MoveTemp(Change), LOCTEXT("EditWidthPoint", "Edit Lane Width Point"));

	Road->UpdateLanes();
	SyncPropertiesFromOffset();
	RequestRebuild();
}

void URoadTool_LaneWidth::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	// OffsetIndex is the tool's own bookkeeping, not something the user edits.
	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URoadTool_LaneWidthProperties, OffsetIndex))
	{
		return;
	}

	ApplyPropertiesToOffset();
}

void URoadTool_LaneWidth::PickUnderRay(const FRay& Ray, URoadBoundary*& OutBoundary, int32& OutIndex) const
{
	OutBoundary = nullptr;
	OutIndex = INDEX_NONE;

	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return;
	}

	URoadBoundary* Selected = GetCurrentBoundary();

	FRoadHitCollector Collector(Ray);
	for (URoadBoundary* Boundary : Road->Boundaries)
	{
		if (Boundary == nullptr)
		{
			continue;
		}

		// The boundary's own line, labelled with no index: what the legacy whole-curve hit proxy meant, and
		// what selecting a boundary without picking one of its control points still means.
		Collector.ConsiderPolyline(Boundary->Curve, Boundary);

		// The control points only exist on screen once their boundary is picked - the legacy Render() drew
		// them with proxies only for the current boundary - so only then are they offered to the ray.
		if (Boundary == Selected)
		{
			for (int32 Index = 0; Index < Boundary->LocalOffsets.Num(); ++Index)
			{
				Collector.ConsiderPoint(Boundary->GetPos(Boundary->LocalOffsets[Index].Dist), Boundary, Index);
			}
		}
	}

	const FRoadRayHit Hit = Collector.Resolve();
	if (Hit.bHit)
	{
		OutBoundary = Cast<URoadBoundary>(Hit.Owner);
		OutIndex = Hit.Index;
	}
}

void URoadTool_LaneWidth::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;
	ARoadActor* Road = GetSelectedRoad();

	// With nothing selected the roads themselves are on screen, so the left button picks one. The legacy
	// tool ran HandleClickRoad() first for the same reason, and it too only answered the left button.
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
		SelectOffset(Boundary, Index);
		return;
	}

	if (Boundary == nullptr)
	{
		return;
	}

	// A right click on a boundary adds a control point where the cursor meets the road, so the station has
	// to come from a real world hit; the legacy tool projected a sentinel here, which landed arbitrarily.
	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		return;
	}

	AddControlPoint(Boundary, Road->GetUV(Position).X);
}

void URoadTool_LaneWidth::SelectParent()
{
	SelectOffset(nullptr, INDEX_NONE);
	Super::SelectParent();
}

void URoadTool_LaneWidth::AddControlPoint(URoadBoundary* Boundary, double Dist)
{
	if (Boundary == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Boundary, GetLocalOffsetsProperty());
	if (Change == nullptr)
	{
		return;
	}

	// AddLocalOffset() answers with the index of the point it inserted, so the new one becomes the
	// selection.
	const int32 NewIndex = Boundary->AddLocalOffset(Dist);
	Change->CaptureAfter();
	EmitArrayChange(Boundary, MoveTemp(Change), LOCTEXT("AddWidthPoint", "Add Lane Width Point"));

	ARoadActor* Road = Boundary->GetRoad();
	if (Road != nullptr)
	{
		Road->UpdateLanes();
	}

	SelectOffset(Boundary, NewIndex);
	RequestRebuild();
}

void URoadTool_LaneWidth::DeleteSelectedOffset()
{
	URoadBoundary* Boundary = GetCurrentBoundary();
	const int32 Index = GetOffsetIndex();
	if (Boundary == nullptr || Index == INDEX_NONE)
	{
		return;
	}

	// A boundary needs at least two control points to describe a width, so the last two stay.
	if (Boundary->LocalOffsets.Num() <= 2)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Boundary, GetLocalOffsetsProperty());
	if (Change == nullptr)
	{
		return;
	}

	Boundary->DeleteOffset(Index);
	Change->CaptureAfter();
	EmitArrayChange(Boundary, MoveTemp(Change), LOCTEXT("DeleteWidthPoint", "Delete Lane Width Point"));

	ARoadActor* Road = Boundary->GetRoad();
	if (Road != nullptr)
	{
		Road->UpdateLanes();
	}

	SelectOffset(nullptr, INDEX_NONE);
	RequestRebuild();
}

FArrayProperty* URoadTool_LaneWidth::GetLocalOffsetsProperty()
{
	return FindFProperty<FArrayProperty>(URoadBoundary::StaticClass(),
		GET_MEMBER_NAME_CHECKED(URoadBoundary, LocalOffsets));
}

void URoadTool_LaneWidth::Render(IToolsContextRenderAPI* RenderAPI)
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

	URoadBoundary* Boundary = GetCurrentBoundary();
	const int32 Index = GetOffsetIndex();

	// Faithful to the legacy pass: every other boundary is drawn plain, and the current one is drawn
	// highlighted below, together with the control points a right click can add to.
	for (URoadBoundary* Other : Road->Boundaries)
	{
		if (Other != nullptr && Other != Boundary)
		{
			DrawCurve(PDI, Other->Curve, RoadToolStyle::Color_Line, RoadToolStyle::Thickness_Line);
		}
	}

	if (Boundary == nullptr)
	{
		return;
	}

	DrawCurve(PDI, Boundary->Curve, RoadToolStyle::Color_Select, RoadToolStyle::Thickness_Line,
		RoadToolStyle::DepthBias_Select);

	for (int32 PointIndex = 0; PointIndex < Boundary->LocalOffsets.Num(); ++PointIndex)
	{
		const double Dist = Boundary->LocalOffsets[PointIndex].Dist;
		const FLinearColor Color = (PointIndex == Index) ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Line;
		DrawPoint(PDI, Boundary, Dist, Color);

		// The legacy tool drew the lane divider across the road at each control point, so the width being
		// changed is readable - but only for boundaries that have a lane inside them, never for the
		// centreline itself.
		if (Boundary != Road->BaseCurve)
		{
			URoadLane* Lane = Boundary->GetSide() ? Boundary->RightLane : Boundary->LeftLane;
			if (Lane != nullptr)
			{
				DrawDivider(PDI, Lane, Dist, Color);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
