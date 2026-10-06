// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_MarkingCurve.h"

#include "InputCoreTypes.h"
#include "RoadBuilderTools.h"
#include "RoadMarking.h"
#include "SceneManagement.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadKeyInputBehavior.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPointGizmo.h"
#include "Tools/RoadPropertyChange.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_MarkingCurve"

UInteractiveTool* URoadTool_MarkingCurveBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_MarkingCurve* NewTool = NewObject<URoadTool_MarkingCurve>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_MarkingCurve::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The drag handle binding, which is what makes the translated handles grabbable; it declines every
	// press that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	DeleteKeyBehavior = NewObject<URoadKeyInputBehavior>();
	DeleteKeyBehavior->Initialize(EKeys::Delete, [this]() { DeleteSelectedMarking(); });
	AddInputBehavior(DeleteKeyBehavior);

	Properties = NewObject<URoadTool_MarkingCurveProperties>(this, TEXT("MarkingCurveSettings"));
	PointProperties = NewObject<URoadTool_MarkingCurvePointProperties>(this, TEXT("MarkingCurvePointSettings"));

	// Nothing is selected yet, so no panel is registered until the first selection.
	RefreshPanel();

	// Points are stored as (station, lateral offset) on the road, so the legacy tool asked for both axes
	// in the road's frame - a translation plane once the gizmo is rotated onto the tangent.
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

void URoadTool_MarkingCurve::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

UMarkingCurve* URoadTool_MarkingCurve::GetCurrentMarking() const
{
	UMarkingCurve* Marking = CurrentMarking.Get();
	ARoadActor* Road = GetSelectedRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return nullptr;
	}

	// A rebuild, an undo or a deleted road can leave the remembered curve behind, so it is only trusted
	// while the road still lists it.
	return Road->Markings.Contains(Marking) ? Marking : nullptr;
}

ARoadActor* URoadTool_MarkingCurve::GetCurrentMarkingRoad() const
{
	UMarkingCurve* Marking = GetCurrentMarking();
	return Marking != nullptr ? Marking->GetRoad() : nullptr;
}

int32 URoadTool_MarkingCurve::GetPointIndex() const
{
	UMarkingCurve* Marking = GetCurrentMarking();
	return (Marking != nullptr && Marking->Points.IsValidIndex(PointIndex)) ? PointIndex : INDEX_NONE;
}

int32 URoadTool_MarkingCurve::GetHandleIndex() const
{
	// Only meaningful for a selected point; the whole-curve case always reports the point handle.
	return GetPointIndex() != INDEX_NONE ? FMath::Clamp(HandleIndex, 0, 2) : 0;
}

void URoadTool_MarkingCurve::SelectMarking(UMarkingCurve* Marking, int32 InPointIndex, int32 InHandleIndex)
{
	// Trusting the caller's index would mean every use site re-testing it; resolving here instead keeps
	// the stored selection either valid or empty.
	const bool bPointValid = (Marking != nullptr) && Marking->Points.IsValidIndex(InPointIndex);
	CurrentMarking = Marking;
	PointIndex = bPointValid ? InPointIndex : INDEX_NONE;
	HandleIndex = bPointValid ? FMath::Clamp(InHandleIndex, 0, 2) : 0;

	SyncProperties();
	RefreshPanel();
	Gizmo->Update(GetCurrentMarking() != nullptr);
	RequestRedraw();
}

void URoadTool_MarkingCurve::RefreshPanel()
{
	// The legacy tool had two mutually exclusive panels - the curve object and one control point struct -
	// and the same exclusivity is reproduced by swapping property sets. Nothing selected means neither.
	UInteractiveToolPropertySet* Wanted = nullptr;
	if (GetCurrentMarking() != nullptr)
	{
		Wanted = (GetPointIndex() != INDEX_NONE) ? static_cast<UInteractiveToolPropertySet*>(PointProperties)
			: static_cast<UInteractiveToolPropertySet*>(Properties);
	}

	if (Wanted == ActivePanel)
	{
		return;
	}

	if (ActivePanel != nullptr)
	{
		RemoveToolPropertySource(ActivePanel);
	}
	ActivePanel = Wanted;
	if (ActivePanel != nullptr)
	{
		AddToolPropertySource(ActivePanel);
	}
}

void URoadTool_MarkingCurve::SyncProperties()
{
	UMarkingCurve* Marking = GetCurrentMarking();
	if (Marking == nullptr)
	{
		Properties->MarkingIndex = INDEX_NONE;
		PointProperties->PointIndex = INDEX_NONE;
		return;
	}

	ARoadActor* Road = Marking->GetRoad();
	Properties->MarkingIndex = (Road != nullptr) ? Road->Markings.IndexOfByKey(Marking) : INDEX_NONE;
	Properties->MarkStyle = Marking->MarkStyle;
	Properties->FillStyle = Marking->FillStyle;
	Properties->Orientation = Marking->Orientation;
	Properties->bClosedLoop = Marking->bClosedLoop;

	const int32 Index = GetPointIndex();
	if (Index == INDEX_NONE)
	{
		PointProperties->PointIndex = INDEX_NONE;
		return;
	}

	const FMarkingCurvePoint& Point = Marking->Points[Index];
	PointProperties->PointIndex = Index;
	PointProperties->HandleIndex = GetHandleIndex();
	PointProperties->Pos = Point.Pos;
	PointProperties->In = Point.In;
	PointProperties->Out = Point.Out;
}

FTransform URoadTool_MarkingCurve::GetMarkingTransform() const
{
	UMarkingCurve* Marking = GetCurrentMarking();
	ARoadActor* Road = GetCurrentMarkingRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return FTransform::Identity;
	}

	// The legacy widget location: the selected handle when there is one, the curve's centroid otherwise -
	// so the whole curve can be slid around as a unit.
	const int32 Index = GetPointIndex();
	const FVector2D UV = (Index != INDEX_NONE)
		? Marking->Points[Index].GetUV(GetHandleIndex())
		: Marking->Center();

	// The legacy GetCustomDrawingCoordinateSystem() reported the road tangent, which is what the gizmo's
	// own rotation reproduces here.
	return FTransform(Road->GetDir(UV.X).Rotation(), Road->GetPos(UV));
}

void URoadTool_MarkingCurve::ApplyMarkingTransform(const FTransform& NewTransform)
{
	UMarkingCurve* Marking = GetCurrentMarking();
	ARoadActor* Road = GetCurrentMarkingRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return;
	}

	const int32 Index = GetPointIndex();
	const FVector2D UV = (Index != INDEX_NONE)
		? Marking->Points[Index].GetUV(GetHandleIndex())
		: Marking->Center();

	// The gizmo reports an absolute position while the curve is stored in road coordinates, so the delta
	// is measured against the data. That makes re-seating the gizmo a no-op and stops the curve drifting
	// away from the cursor that is dragging it.
	const FVector Delta = NewTransform.GetLocation() - Road->GetPos(UV);
	if (Delta.IsNearlyZero())
	{
		return;
	}

	// The legacy tool decomposed its drag with the custom coordinate system it had set to the road
	// tangent, then added the components straight onto the stored coordinates - which is the road frame,
	// and what ApplyDelta() does per handle: 0 the point, 1 In, 2 Out.
	const FVector2D LocalDelta = ToRoadFrame(Road, UV.X, Delta);
	if (Index != INDEX_NONE)
	{
		Marking->Points[Index].ApplyDelta(GetHandleIndex(), LocalDelta);
	}
	else
	{
		for (FMarkingCurvePoint& Point : Marking->Points)
		{
			Point.ApplyDelta(0, LocalDelta);
		}
	}

	// No explicit undo entry: the gizmo's own change replays this edit through the proxy.
	SyncProperties();

	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_MarkingCurve::RebuildAfterDrag()
{
	RequestRebuild();
}

FInputRayHit URoadTool_MarkingCurve::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// The gizmo shows for a selected point and for a whole selected curve alike, and nothing to measure
	// against without a scale. The scale is the gizmo's own: it is measured in Render() every frame, the
	// frame the user aimed at.
	UMarkingCurve* Marking = GetCurrentMarking();
	const double PixelToWorld = (Gizmo != nullptr) ? Gizmo->GetLastPixelToWorld() : 0.0;
	if (Gizmo == nullptr || Marking == nullptr || GetCurrentMarkingRoad() == nullptr || PixelToWorld <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, PixelToWorld);

	// The handle test's own verdict, per tool, so a press that fails to start a drag says whether it found
	// no handle or was overruled downstream. Verbose: one line per press, not per move.
	UE_LOG(LogRoadBuilder, Verbose, TEXT("drag markingcurve handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes in ApplyMarkingTransform record nothing themselves.
	DragChange = FRoadArrayChange::CaptureBefore(Marking, GetPointsProperty());

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit; the depth keeps this
	// behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_MarkingCurve::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	if (DraggedHandle == ERoadGizmoHandle::None || Gizmo == nullptr)
	{
		return;
	}

	// The gizmo measures the cursor's motion in the subspace it fixed at the press and calls back into
	// ApplyMarkingTransform, so the existing write path - the road-frame decomposition, the handle
	// dispatch and the deferred rebuild - is all reused.
	Gizmo->DragHandle(DraggedHandle, DragPos.WorldRay);
}

void URoadTool_MarkingCurve::OnRoadDragEnded()
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
		if (UMarkingCurve* Marking = GetCurrentMarking())
		{
			DragChange->CaptureAfter();
			EmitArrayChange(Marking, MoveTemp(DragChange),
				LOCTEXT("MoveMarkingCurvePoint", "Move Marking Curve Point"));
		}
		DragChange.Reset();
	}
}

void URoadTool_MarkingCurve::ApplyProperties(UObject* PropertySet, FProperty* Property)
{
	UMarkingCurve* Marking = GetCurrentMarking();
	if (Marking == nullptr || Property == nullptr)
	{
		return;
	}

	const FName ChangedName = Property->GetFName();

	if (PropertySet == PointProperties)
	{
		const int32 Index = GetPointIndex();
		if (Index == INDEX_NONE)
		{
			return;
		}

		TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Marking, GetPointsProperty());
		if (Change == nullptr)
		{
			return;
		}

		FMarkingCurvePoint& Point = Marking->Points[Index];
		if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurvePointProperties, Pos))
		{
			Point.Pos = PointProperties->Pos;
		}
		else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurvePointProperties, In))
		{
			Point.In = PointProperties->In;
		}
		else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurvePointProperties, Out))
		{
			Point.Out = PointProperties->Out;
		}

		Change->CaptureAfter();
		EmitArrayChange(Marking, MoveTemp(Change), LOCTEXT("EditMarkingCurvePoint", "Edit Marking Curve Point"));
	}
	else if (PropertySet == Properties)
	{
		// MarkingIndex is the tool's own bookkeeping, not something the user edits.
		if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurveProperties, MarkingIndex))
		{
			return;
		}

		// The panel property and the model field are matched by name, so the record describes exactly what
		// is about to change rather than the whole object.
		FProperty* Target = FindFProperty<FProperty>(UMarkingCurve::StaticClass(), ChangedName);
		if (Target == nullptr)
		{
			return;
		}

		TUniquePtr<FRoadPropertyChange> Change = FRoadPropertyChange::CaptureBefore(Marking, Target);
		if (Change == nullptr)
		{
			return;
		}

		if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurveProperties, MarkStyle))
		{
			Marking->MarkStyle = Properties->MarkStyle;
		}
		else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurveProperties, FillStyle))
		{
			Marking->FillStyle = Properties->FillStyle;
		}
		else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurveProperties, Orientation))
		{
			Marking->Orientation = Properties->Orientation;
		}
		else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_MarkingCurveProperties, bClosedLoop))
		{
			Marking->bClosedLoop = Properties->bClosedLoop;
		}

		Change->CaptureAfter();
		EmitPropertyChange(Marking, MoveTemp(Change), LOCTEXT("EditMarkingCurve", "Edit Marking Curve"));
	}
	else
	{
		return;
	}

	RequestRebuild();
}

void URoadTool_MarkingCurve::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (Property == nullptr || (PropertySet != Properties && PropertySet != PointProperties))
	{
		return;
	}

	// PointIndex and HandleIndex are the tool's own bookkeeping, not something the user edits; the
	// ApplyProperties() branch for the point set ignores them by falling through its name tests.
	ApplyProperties(PropertySet, Property);
}

void URoadTool_MarkingCurve::PickUnderRay(const FRay& Ray, UMarkingCurve*& OutMarking, int32& OutPointIndex,
	int32& OutHandleIndex) const
{
	OutMarking = nullptr;
	OutPointIndex = INDEX_NONE;
	OutHandleIndex = 0;

	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return;
	}

	UMarkingCurve* Selected = GetCurrentMarking();
	const int32 SelectedPoint = GetPointIndex();

	FRoadHitCollector Collector(Ray);
	for (URoadMarking* Marking : Road->Markings)
	{
		UMarkingCurve* Curve = Cast<UMarkingCurve>(Marking);
		if (Curve == nullptr)
		{
			continue;
		}

		// The curve's own line, labelled with no index: what the legacy whole-curve hit proxy meant.
		Collector.ConsiderPolyline(Curve->CreatePolyline(), Curve);

		// The handles only exist on screen once their curve is picked, and the legacy Render() drew all
		// three only for the selected point - so only those are offered to the ray.
		if (Curve != Selected)
		{
			continue;
		}

		for (int32 Point = 0; Point < Curve->Points.Num(); ++Point)
		{
			const int32 HandleCount = (Point == SelectedPoint) ? 3 : 1;
			for (int32 Handle = 0; Handle < HandleCount; ++Handle)
			{
				Collector.ConsiderPoint(Road->GetPos(Curve->Points[Point].GetUV(Handle)), Curve, Point, Handle);
			}
		}
	}

	const FRoadRayHit Hit = Collector.Resolve();
	if (!Hit.bHit)
	{
		return;
	}

	OutMarking = Cast<UMarkingCurve>(Hit.Owner);
	OutPointIndex = Hit.Index;
	OutHandleIndex = (Hit.SubIndex != INDEX_NONE) ? Hit.SubIndex : 0;
}

void URoadTool_MarkingCurve::AddMarkingCurveAt(ARoadActor* Road, const FRay& Ray)
{
	// A right click needs the cursor to be over the road, so the point has to come from a real world hit;
	// the legacy tool projected a sentinel here, which landed arbitrarily.
	const FVector Position = LineTrace(Ray);
	if (Road == nullptr || Position.X >= WORLD_MAX)
	{
		return;
	}

	UMarkingCurve* Marking = nullptr;

	// AddMarkingCurve() creates an object and attaches it to the road, so this is not a change to a single
	// object and no FToolCommandChange can describe it. The host transaction can - the same reason the
	// legacy tool opened an FScopedTransaction here.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("MarkingCurve", "Marking Curve"));
		Road->Modify();
		Marking = Road->AddMarkingCurve();
	}

	// The first point goes through the same array edit every later point does, so it lands in the same
	// transaction bracket the host just opened.
	CurrentMarking = Marking;
	PointIndex = INDEX_NONE;
	HandleIndex = 0;
	InsertPointAt(Road, Ray);

	RequestRebuild();
}

void URoadTool_MarkingCurve::InsertPointAt(ARoadActor* Road, const FRay& Ray)
{
	UMarkingCurve* Marking = GetCurrentMarking();
	if (Marking == nullptr || Road == nullptr)
	{
		return;
	}

	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Marking, GetPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	// InsertPoint() answers through the index it was given: it appends when the caller pointed at the last
	// point (which is also the empty-curve case, where the index is INDEX_NONE) and otherwise inserts
	// before it, leaving the index naming the new point. PointIndex is a member for exactly that reason.
	Marking->InsertPoint(Road->GetUV(Position), PointIndex);

	Change->CaptureAfter();
	EmitArrayChange(Marking, MoveTemp(Change), LOCTEXT("AddMarkingCurvePoint", "Add Marking Curve Point"));

	SelectMarking(Marking, PointIndex, 0);
	RequestRebuild();
}

void URoadTool_MarkingCurve::CloseCurrentMarking()
{
	UMarkingCurve* Marking = GetCurrentMarking();
	if (Marking == nullptr || Marking->Points.Num() == 0)
	{
		return;
	}

	// MakeClose() edits two properties at once - the endpoint tangents and the closed-loop flag - so the
	// two changes are bracketed as one undo step rather than landing as two.
	FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("MarkingCurve", "Marking Curve"));

	TUniquePtr<FRoadArrayChange> PointChange = FRoadArrayChange::CaptureBefore(Marking, GetPointsProperty());
	TUniquePtr<FRoadPropertyChange> LoopChange = FRoadPropertyChange::CaptureBefore(Marking, GetClosedLoopProperty());

	Marking->MakeClose();

	if (PointChange != nullptr)
	{
		PointChange->CaptureAfter();
		EmitArrayChange(Marking, MoveTemp(PointChange), LOCTEXT("CloseMarkingCurve", "Close Marking Curve"));
	}
	if (LoopChange != nullptr)
	{
		LoopChange->CaptureAfter();
		EmitPropertyChange(Marking, MoveTemp(LoopChange), LOCTEXT("CloseMarkingCurve", "Close Marking Curve"));
	}

	SyncProperties();
	RequestRebuild();
}

void URoadTool_MarkingCurve::DeleteSelectedMarking()
{
	UMarkingCurve* Marking = GetCurrentMarking();
	ARoadActor* Road = GetSelectedRoad();
	if (Marking == nullptr || Road == nullptr)
	{
		return;
	}

	// DeleteMarking() destroys the object, so it needs the same structural bracket AddMarkingCurve() does.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("MarkingCurve", "Marking Curve"));
		Road->Modify();
		Road->DeleteMarking(Marking);
	}

	// Self-reference goes with the object; without clearing it the panel would keep showing a dead one.
	SelectMarking(nullptr, INDEX_NONE, 0);
	RequestRebuild();
}

void URoadTool_MarkingCurve::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
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

	UMarkingCurve* HitMarking = nullptr;
	int32 HitPointIndex = INDEX_NONE;
	int32 HitHandleIndex = 0;
	PickUnderRay(Ray, HitMarking, HitPointIndex, HitHandleIndex);

	if (!bRightButton)
	{
		// A miss clears the selection, which is how the legacy tool emptied its panel.
		SelectMarking(HitMarking, HitPointIndex, HitHandleIndex);
		return;
	}

	// A right click means different things depending on whether the selected endpoint is being extended,
	// which is the branch the legacy tool took too.
	UMarkingCurve* Marking = GetCurrentMarking();
	if (Marking != nullptr && Marking->IsEndPoint(PointIndex))
	{
		if (HitMarking != nullptr)
		{
			// Clicking the far endpoint of the same curve closes it; clicking any other handle reselects.
			if (HitMarking == Marking && HitPointIndex != PointIndex && Marking->IsEndPoint(HitPointIndex))
			{
				CloseCurrentMarking();
			}
			else
			{
				SelectMarking(HitMarking, HitPointIndex, HitHandleIndex);
			}
		}
		else
		{
			InsertPointAt(Road, Ray);
		}
		return;
	}

	if (HitMarking != nullptr)
	{
		SelectMarking(HitMarking, HitPointIndex, HitHandleIndex);
		return;
	}

	AddMarkingCurveAt(Road, Ray);
}

void URoadTool_MarkingCurve::SelectParent()
{
	SelectMarking(nullptr, INDEX_NONE, 0);
	Super::SelectParent();
}

FArrayProperty* URoadTool_MarkingCurve::GetPointsProperty()
{
	return FindFProperty<FArrayProperty>(UMarkingCurve::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UMarkingCurve, Points));
}

FBoolProperty* URoadTool_MarkingCurve::GetClosedLoopProperty()
{
	return FindFProperty<FBoolProperty>(UMarkingCurve::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UMarkingCurve, bClosedLoop));
}

void URoadTool_MarkingCurve::Render(IToolsContextRenderAPI* RenderAPI)
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
		// No road chosen yet, so offer all of them - including the ramps a junction owns, which is what
		// the legacy links pass drew.
		DrawRoads(PDI, /*bDrawLinks*/ true);
		return;
	}

	UMarkingCurve* Marking = GetCurrentMarking();
	const int32 SelectedPoint = GetPointIndex();

	for (URoadMarking* Other : Road->Markings)
	{
		UMarkingCurve* Curve = Cast<UMarkingCurve>(Other);
		if (Curve == nullptr)
		{
			continue;
		}

		const bool bCurrent = (Curve == Marking);
		DrawCurve(PDI, Curve->CreatePolyline(),
			bCurrent ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Line, RoadToolStyle::Thickness_Road);

		if (!bCurrent)
		{
			continue;
		}

		// Faithful to the legacy pass: every point shows its position handle, and only the selected point
		// additionally shows its two tangent handles, tied back to the point they belong to.
		for (int32 Point = 0; Point < Curve->Points.Num(); ++Point)
		{
			const int32 HandleCount = (Point == SelectedPoint) ? 3 : 1;
			for (int32 Handle = 0; Handle < HandleCount; ++Handle)
			{
				const FVector Position = Road->GetPos(Curve->Points[Point].GetUV(Handle));
				PDI->DrawPoint(Position,
					(Point == SelectedPoint) ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Line,
					RoadToolStyle::Size_Point, SDPG_Foreground);

				if (Handle > 0)
				{
					PDI->DrawLine(Position, Road->GetPos(Curve->Points[Point].GetUV(0)),
						RoadToolStyle::Color_Select, SDPG_Foreground, RoadToolStyle::Thickness_Line);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
