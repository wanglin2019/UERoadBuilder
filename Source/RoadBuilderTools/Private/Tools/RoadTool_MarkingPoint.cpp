// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_MarkingPoint.h"

#include "InputCoreTypes.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "RoadMarking.h"
#include "SceneManagement.h"
#include "Tools/RoadKeyInputBehavior.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPointGizmo.h"
#include "Tools/RoadPropertyChange.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_MarkingPoint"

UInteractiveTool* URoadTool_MarkingPointBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_MarkingPoint* NewTool = NewObject<URoadTool_MarkingPoint>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_MarkingPoint::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The drag handle binding, which is what makes the translated handles grabbable; it declines every
	// press that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	DeleteKeyBehavior = NewObject<URoadKeyInputBehavior>();
	DeleteKeyBehavior->Initialize(EKeys::Delete, [this]() { DeleteSelectedMarking(); });
	AddInputBehavior(DeleteKeyBehavior);

	Properties = NewObject<URoadTool_MarkingPointProperties>(this, TEXT("MarkingPointSettings"));
	AddToolPropertySource(Properties);

	// The marking is stored as (station, lateral offset) on the road, so the legacy tool asked for both
	// axes in the road's frame - a translation plane once the gizmo is rotated onto the tangent.
	//
	// Plane + both axes: the plane normal follows the road frame, so the rectangle can go edge-on and
	// cull itself for some headings; the axis arrows are the dependable half. See URoadTool_RoadPlan::Setup.
	Gizmo = NewObject<URoadPointGizmo>(this);
	Gizmo->Initialize(this,
		ETransformGizmoSubElements::TranslatePlaneXY |
		ETransformGizmoSubElements::TranslateAxisX |
		ETransformGizmoSubElements::TranslateAxisY,
		[this]() { return GetMarkingTransform(); },
		[this](const FTransform& NewTransform) { ApplyMarkingTransform(NewTransform); },
		[this]() { RebuildAfterDrag(); });
}

void URoadTool_MarkingPoint::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

UMarkingPoint* URoadTool_MarkingPoint::GetCurrentMarking() const
{
	UMarkingPoint* Marking = CurrentMarking.Get();
	ARoadActor* Road = GetSelectedRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return nullptr;
	}

	// A rebuild, an undo or a deleted road can leave the remembered marking behind, so it is only trusted
	// while the road still lists it.
	return Road->Markings.Contains(Marking) ? Marking : nullptr;
}

ARoadActor* URoadTool_MarkingPoint::GetCurrentMarkingRoad() const
{
	UMarkingPoint* Marking = GetCurrentMarking();
	return Marking != nullptr ? Marking->GetRoad() : nullptr;
}

void URoadTool_MarkingPoint::SelectMarking(UMarkingPoint* Marking)
{
	CurrentMarking = Marking;
	SyncPropertiesFromMarking();
	Gizmo->Update(GetCurrentMarking() != nullptr);

	// Only the highlight changed, so a repaint is enough; the legacy Reset() invalidated the view for the
	// same reason and ran no rebuild.
	RequestRedraw();
}

void URoadTool_MarkingPoint::SyncPropertiesFromMarking()
{
	UMarkingPoint* Marking = GetCurrentMarking();
	if (Marking == nullptr)
	{
		Properties->MarkingIndex = INDEX_NONE;
		Properties->Mesh = nullptr;
		Properties->Point = FVector2D::ZeroVector;
		return;
	}

	ARoadActor* Road = Marking->GetRoad();
	Properties->MarkingIndex = (Road != nullptr) ? Road->Markings.IndexOfByKey(Marking) : INDEX_NONE;
	Properties->Mesh = Marking->Mesh;
	Properties->Point = Marking->Point;
}

FTransform URoadTool_MarkingPoint::GetMarkingTransform() const
{
	UMarkingPoint* Marking = GetCurrentMarking();
	ARoadActor* Road = GetCurrentMarkingRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return FTransform::Identity;
	}

	// The legacy GetCustomDrawingCoordinateSystem() reported the road tangent, which is what the gizmo's
	// own rotation reproduces here.
	return FTransform(Road->GetDir(Marking->Point.X).Rotation(), Road->GetPos(Marking->Point));
}

void URoadTool_MarkingPoint::ApplyMarkingTransform(const FTransform& NewTransform)
{
	UMarkingPoint* Marking = GetCurrentMarking();
	ARoadActor* Road = GetCurrentMarkingRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return;
	}

	// The gizmo reports an absolute position while the marking is stored as (station, offset), so the
	// delta is measured against the data. That makes re-seating the gizmo a no-op and keeps the marking
	// from drifting away from the cursor that is dragging it.
	const FVector Delta = NewTransform.GetLocation() - Road->GetPos(Marking->Point);
	if (Delta.IsNearlyZero())
	{
		return;
	}

	// The legacy tool split its drag with the custom coordinate system it had set to the road tangent,
	// then added both components straight onto the stored UV - which is what the road frame gives here.
	const FVector2D LocalDelta = ToRoadFrame(Road, Marking->Point.X, Delta);
	Marking->Point += LocalDelta;

	// No explicit undo entry: the gizmo's own change replays this edit through the proxy.
	SyncPropertiesFromMarking();

	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_MarkingPoint::RebuildAfterDrag()
{
	RequestRebuild();
}

FInputRayHit URoadTool_MarkingPoint::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Nothing to grab without a selected marking, and nothing to measure against without a scale. The
	// scale is the gizmo's own: it is measured in Render() every frame, the frame the user aimed at.
	UMarkingPoint* Marking = GetCurrentMarking();
	const double PixelToWorld = (Gizmo != nullptr) ? Gizmo->GetLastPixelToWorld() : 0.0;
	if (Gizmo == nullptr || Marking == nullptr || GetCurrentMarkingRoad() == nullptr || PixelToWorld <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, PixelToWorld);

	// The handle test's own verdict, per tool, so a press that fails to start a drag says whether it found
	// no handle or was overruled downstream. Verbose: one line per press, not per move.
	RoadLog_Debug(TEXT("drag markingpoint handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes in ApplyMarkingTransform record nothing themselves.
	FProperty* PointProperty = FindFProperty<FProperty>(UMarkingPoint::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UMarkingPoint, Point));
	DragChange = FRoadPropertyChange::CaptureBefore(Marking, PointProperty);

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit; the depth keeps this
	// behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_MarkingPoint::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	if (DraggedHandle == ERoadGizmoHandle::None || Gizmo == nullptr)
	{
		return;
	}

	// The gizmo measures the cursor's motion in the subspace it fixed at the press and calls back into
	// ApplyMarkingTransform, so the existing write path - the road-frame decomposition and the deferred
	// rebuild included - is all reused.
	Gizmo->DragHandle(DraggedHandle, DragPos.WorldRay);
}

void URoadTool_MarkingPoint::OnRoadDragEnded()
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
		if (UMarkingPoint* Marking = GetCurrentMarking())
		{
			DragChange->CaptureAfter();
			EmitPropertyChange(Marking, MoveTemp(DragChange), LOCTEXT("MoveMarkingPoint", "Move Marking Point"));
		}
		DragChange.Reset();
	}
}

void URoadTool_MarkingPoint::ApplyPropertiesToMarking(FProperty* Property)
{
	UMarkingPoint* Marking = GetCurrentMarking();
	if (Marking == nullptr || Property == nullptr)
	{
		return;
	}

	// The panel property and the model field are matched by name, so the record describes exactly what is
	// about to change rather than the whole object.
	FProperty* Target = FindFProperty<FProperty>(UMarkingPoint::StaticClass(), Property->GetFName());
	if (Target == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadPropertyChange> Change = FRoadPropertyChange::CaptureBefore(Marking, Target);
	if (Change == nullptr)
	{
		return;
	}

	const FName ChangedName = Property->GetFName();
	if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingPointProperties, Mesh))
	{
		Marking->Mesh = Properties->Mesh;
	}
	else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingPointProperties, Point))
	{
		Marking->Point = Properties->Point;
	}

	Change->CaptureAfter();
	EmitPropertyChange(Marking, MoveTemp(Change), LOCTEXT("EditMarkingPoint", "Edit Marking Point"));

	RequestRebuild();
}

void URoadTool_MarkingPoint::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	// MarkingIndex is the tool's own bookkeeping, not something the user edits.
	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingPointProperties, MarkingIndex))
	{
		return;
	}

	ApplyPropertiesToMarking(Property);
}

UMarkingPoint* URoadTool_MarkingPoint::PickMarkingUnderRay(const FRay& Ray) const
{
	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return nullptr;
	}

	// Only the point markings of the selected road are on offer, because those are the ones its Render()
	// drew - the same restriction the legacy hit-proxy pass had.
	FRoadHitCollector Collector(Ray);
	for (URoadMarking* Marking : Road->Markings)
	{
		UMarkingPoint* Point = Cast<UMarkingPoint>(Marking);
		if (Point == nullptr)
		{
			continue;
		}

		Collector.ConsiderPoint(Road->GetPos(Point->Point), Point);
	}

	const FRoadRayHit Hit = Collector.Resolve();
	return Hit.bHit ? Cast<UMarkingPoint>(Hit.Owner) : nullptr;
}

bool URoadTool_MarkingPoint::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
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
			return true;
		}
		return false;
	}

	if (Road->Length() <= 0.0)
	{
		return false;
	}

	if (!bRightButton)
	{
		// A miss clears the selection, which is how the legacy tool emptied its panel.
		SelectMarking(PickMarkingUnderRay(Ray));
		return true;
	}

	// A right click drops a new marking where the cursor meets the road, so the position has to come from a
	// real world hit; the legacy tool projected a sentinel here, landing arbitrarily.
	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		return false;
	}

	AddMarkingPointAt(Road, Road->GetUV(Position));
	return true;
}

void URoadTool_MarkingPoint::SelectParent()
{
	SelectMarking(nullptr);
	Super::SelectParent();
}

void URoadTool_MarkingPoint::AddMarkingPointAt(ARoadActor* Road, const FVector2D& UV)
{
	if (Road == nullptr)
	{
		return;
	}

	UMarkingPoint* Marking = nullptr;

	// AddMarkingPoint() creates an object and attaches it to the road, so this is not a change to a single
	// object and no FToolCommandChange can describe it. The host transaction can - which is the one place
	// the legacy tool reached for FScopedTransaction too.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("MarkingPoint", "Marking Point"));
		Road->Modify();
		Marking = Road->AddMarkingPoint(UV);
	}

	SelectMarking(Marking);
	RequestRebuild();
}

void URoadTool_MarkingPoint::DeleteSelectedMarking()
{
	UMarkingPoint* Marking = GetCurrentMarking();
	ARoadActor* Road = GetSelectedRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return;
	}

	// DeleteMarking() destroys the object, so it needs the same structural bracket AddMarkingPoint() does.
	// Not every host can undo an object destruction; the interface is what lets a host say so.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("MarkingPoint", "Marking Point"));
		Road->Modify();
		Road->DeleteMarking(Marking);
	}

	// Self-reference goes with the object; without clearing it the panel would keep showing a dead one.
	SelectMarking(nullptr);
	RequestRebuild();
}

void URoadTool_MarkingPoint::Render(IToolsContextRenderAPI* RenderAPI)
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
		// No road chosen yet, so offer all of them - including the ramps a junction owns, which is what the
		// legacy links pass drew.
		DrawRoads(PDI, /*bDrawLinks*/ true);
		return;
	}

	UMarkingPoint* Marking = GetCurrentMarking();
	for (URoadMarking* Other : Road->Markings)
	{
		UMarkingPoint* Point = Cast<UMarkingPoint>(Other);
		if (Point == nullptr)
		{
			continue;
		}

		PDI->DrawPoint(Road->GetPos(Point->Point),
			(Point == Marking) ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Line,
			RoadToolStyle::Size_Point, SDPG_Foreground);
	}
}

#undef LOCTEXT_NAMESPACE
