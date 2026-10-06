// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_RoadPlan.h"

#include "BaseGizmos/GizmoRenderingUtil.h"
#include "InputCoreTypes.h"
#include "InteractiveToolManager.h"
#include "RoadBoundary.h"
#include "RoadScene.h"
#include "SceneManagement.h"
#include "Settings.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadKeyInputBehavior.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPointGizmo.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_RoadPlan"

UInteractiveTool* URoadTool_RoadPlanBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_RoadPlan* NewTool = NewObject<URoadTool_RoadPlan>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_RoadPlan::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The drag handle binding, which is what makes the translated handles grabbable; it declines every
	// press that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	DeleteKeyBehavior = NewObject<URoadKeyInputBehavior>();
	DeleteKeyBehavior->Initialize(EKeys::Delete, [this]() { DeleteSelection(); });
	AddInputBehavior(DeleteKeyBehavior);

	// The plan settings object, exactly as the legacy mode handed it to its inspector: it carries the
	// style and base height a new road is created from, and its Create/Apply actions are CallInEditor
	// UFUNCTIONs, which the property panel renders as buttons.
	AddToolPropertySource(GetMutableDefault<USettings_RoadPlan>());

	Properties = NewObject<URoadTool_RoadPlanProperties>(this, TEXT("RoadPlanSettings"));
	AddToolPropertySource(Properties);

	// Alignment points are stored in world X/Y, and the legacy tool never overrode
	// GetCustomDrawingCoordinateSystem, so its widget stayed on the world axes and a drag arrived in world
	// space - which is why this gizmo is not rotated onto a road frame the way the cross-section tools are.
	//
	// The gizmo supplies hit testing and the drag, but not the visuals: UCombinedTransformGizmo has no
	// Render() override, so its handles only reach the screen through the ACombinedTransformGizmoActor it
	// spawns, which this tool's view never draws. The handles are therefore drawn in immediate mode by
	// DrawTranslateHandles, and this gizmo is asked for exactly the three elements that call draws:
	// plane, X and Y. Asking for the same set also keeps the grabbable regions and the drawn handles in
	// agreement, which is why the plane element stays even though only the arrows are strictly needed.
	Gizmo = NewObject<URoadPointGizmo>(this);
	Gizmo->Initialize(this,
		ETransformGizmoSubElements::TranslatePlaneXY |
		ETransformGizmoSubElements::TranslateAxisX |
		ETransformGizmoSubElements::TranslateAxisY,
		[this]() { return GetPointTransform(); },
		[this](const FTransform& NewTransform) { ApplyPointTransform(NewTransform); },
		[this]() { RebuildAfterDrag(); });

	// DIAGNOSTIC (remove when the click path is confirmed): records what the host gave this tool at the
	// moment it started. A null scene here is the "nothing works" case; a valid one moves the search to
	// the input path. The behaviours registered above are reported separately, by the base class.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [2 tool] RoadPlan Setup manager=%d scene=%d roads=%d selectedRoad=%d"),
		GetToolManager() != nullptr ? 1 : 0,
		GetRoadScene() != nullptr ? 1 : 0,
		GetRoadScene() != nullptr ? GetRoadScene()->Roads.Num() : -1,
		GetSelectedRoad() != nullptr ? 1 : 0);

	SyncProperties();
}

void URoadTool_RoadPlan::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

int32 URoadTool_RoadPlan::GetPointIndex() const
{
	const ARoadActor* Road = GetSelectedRoad();
	return (Road != nullptr && Road->RoadPoints.IsValidIndex(PointIndex)) ? PointIndex : INDEX_NONE;
}

void URoadTool_RoadPlan::SelectRoadAndPoint(ARoadActor* Road, int32 Index)
{
	SetSelectedRoad(Road);
	PointIndex = (Road != nullptr && Road->RoadPoints.IsValidIndex(Index)) ? Index : INDEX_NONE;

	SyncProperties();

	// DIAGNOSTIC (remove when the drag path is confirmed): Index is what the click asked for and PointIndex
	// is what survived validation, so a discrepancy here means the hit index and the road's own array
	// disagree - which would hide the gizmo even though the click looked good.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [6 gizmo] SelectRoadAndPoint road=%d in=%d stored=%d hasTarget=%d"),
		Road != nullptr ? 1 : 0, Index, PointIndex, GetPointIndex() != INDEX_NONE ? 1 : 0);

	Gizmo->Update(GetPointIndex() != INDEX_NONE);
	RequestRedraw();
}

void URoadTool_RoadPlan::ResetSelection()
{
	// The legacy Reset() cleared both halves and emptied the panel; the host owns the road half of that.
	SetSelectedRoad(nullptr);
	PointIndex = INDEX_NONE;

	SyncProperties();
	Gizmo->Update(false);
	RequestRedraw();
}

void URoadTool_RoadPlan::SyncProperties()
{
	const ARoadActor* Road = GetSelectedRoad();
	const int32 Index = GetPointIndex();
	if (Road == nullptr || Index == INDEX_NONE)
	{
		Properties->PointIndex = INDEX_NONE;
		Properties->Dist = 0.0;
		Properties->Pos = FVector2D::ZeroVector;
		Properties->MaxRadius = 50000.0;
		Properties->CurvatureBlend = 0.0;
		return;
	}

	const FRoadPoint& Point = Road->RoadPoints[Index];
	Properties->PointIndex = Index;
	Properties->Dist = Point.Dist;
	Properties->Pos = Point.Pos;
	Properties->MaxRadius = Point.MaxRadius;
	Properties->CurvatureBlend = Point.CurvatureBlend;
}

FTransform URoadTool_RoadPlan::GetPointTransform() const
{
	ARoadActor* Road = GetSelectedRoad();
	const int32 Index = GetPointIndex();
	if (Road == nullptr || Index == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	// Identity rotation: the legacy widget was never re-oriented, so a drag arrives in world space.
	return FTransform(Road->GetPos(Index));
}

void URoadTool_RoadPlan::ApplyPointTransform(const FTransform& NewTransform)
{
	ARoadActor* Road = GetSelectedRoad();
	const int32 Index = GetPointIndex();
	if (Road == nullptr || Index == INDEX_NONE)
	{
		return;
	}

	// The gizmo reports an absolute position while the point is stored as plain world X/Y, so the delta is
	// measured against the data: re-seating the gizmo is then a no-op and the point cannot drift away from
	// the cursor. Taking only X and Y is what the legacy cast to FVector2D did.
	const FVector Delta = NewTransform.GetLocation() - Road->GetPos(Index);
	if (Delta.IsNearlyZero())
	{
		return;
	}

	Road->RoadPoints[Index].Pos += FVector2D(Delta.X, Delta.Y);
	UpdateEndpointConnection(Road, Index);

	// The fitted curve is recomputed on every step, exactly as the legacy InputDelta did; the generated
	// geometry waits for the drag to end, which is what its LazyRebuild flag deferred.
	Road->UpdateCurve();
	SyncProperties();

	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_RoadPlan::UpdateEndpointConnection(ARoadActor* Road, int32 Index)
{
	if (Road == nullptr || !Road->RoadPoints.IsValidIndex(Index))
	{
		return;
	}

	// Only an endpoint can be connected to another road, and connecting is exclusive: whatever the point
	// was attached to before is released first.
	if (Index != 0 && Index != Road->RoadPoints.Num() - 1)
	{
		return;
	}

	Road->DisconnectAll(Index);

	// The search is done at the height the point sits at, which is the endpoint's own height - the first
	// alignment point uses the start of the height profile and the last one its end.
	const int32 HeightIndex = (Index != 0) ? Road->HeightPoints.Num() - 1 : 0;
	const double Height = Road->HeightPoints.IsValidIndex(HeightIndex) ? Road->HeightPoints[HeightIndex].Height : 0.0;

	ARoadScene* Scene = GetRoadScene();
	if (Scene == nullptr)
	{
		return;
	}

	ARoadActor* HoveredRoad = Scene->PickRoad(FVector(Road->RoadPoints[Index].Pos, Height), Road);
	if (HoveredRoad != nullptr && HoveredRoad != Road)
	{
		Road->ConnectTo(Index, HoveredRoad);
	}
}

void URoadTool_RoadPlan::RebuildAfterDrag()
{
	RequestRebuild();
}

void URoadTool_RoadPlan::ApplyProperties(FProperty* Property)
{
	ARoadActor* Road = GetSelectedRoad();
	const int32 Index = GetPointIndex();
	if (Road == nullptr || Index == INDEX_NONE || Property == nullptr)
	{
		return;
	}

	const FName ChangedName = Property->GetFName();
	if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_RoadPlanProperties, PointIndex) ||
		ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_RoadPlanProperties, Dist))
	{
		// PointIndex is the tool's bookkeeping and Dist is written by the curve fitting, so neither is a
		// user edit worth recording.
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Road, GetRoadPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	FRoadPoint& Point = Road->RoadPoints[Index];
	if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_RoadPlanProperties, Pos))
	{
		Point.Pos = Properties->Pos;
	}
	else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_RoadPlanProperties, MaxRadius))
	{
		Point.MaxRadius = Properties->MaxRadius;
	}
	else if (ChangedName == GET_MEMBER_NAME_CHECKED(URoadTool_RoadPlanProperties, CurvatureBlend))
	{
		Point.CurvatureBlend = Properties->CurvatureBlend;
	}

	Change->CaptureAfter();
	EmitArrayChange(Road, MoveTemp(Change), LOCTEXT("EditRoadPlanPoint", "Edit Road Plan Point"));

	// The legacy NotifyHook did exactly this pair: refit the curve, then regenerate.
	Road->UpdateCurve();
	SyncProperties();
	RequestRebuild();
}

void URoadTool_RoadPlan::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	ApplyProperties(Property);
}

FVector URoadTool_RoadPlan::GetPlanPosition(const FRay& Ray) const
{
	// The alignment is laid out on the horizontal plane the plan settings put it on, so a click resolves
	// against that plane rather than against whatever geometry the ray happens to meet.
	const USettings_RoadPlan* Data = GetDefault<USettings_RoadPlan>();
	const double BaseHeight = (Data != nullptr) ? Data->BaseHeight : 0.0;
	return FMath::RayPlaneIntersection(Ray.Origin, Ray.Direction,
		FPlane(FVector(0.0, 0.0, BaseHeight), FVector::UpVector));
}

void URoadTool_RoadPlan::PickUnderRay(const FRay& Ray, ARoadActor*& OutRoad, int32& OutPointIndex) const
{
	OutRoad = nullptr;
	OutPointIndex = INDEX_NONE;

	ARoadScene* Scene = GetRoadScene();
	if (Scene == nullptr)
	{
		return;
	}

	// One collector for both kinds of element, so the nearer one wins. That is the geometry the legacy
	// Render() drew - every road, then the selected road's points on top of it - resolved by distance
	// instead of by draw order.
	FRoadHitCollector Collector(Ray);
	for (ARoadActor* Road : Scene->Roads)
	{
		if (Road == nullptr || Road->BaseCurve == nullptr)
		{
			continue;
		}

		Collector.ConsiderPolyline(Road->BaseCurve->Curve, Road);
		if (Road->BaseCurve->Curve.Points.Num() == 1)
		{
			Collector.ConsiderPoint(Road->BaseCurve->Curve.Points[0].Pos, Road);
		}
	}

	ARoadActor* SelectedRoad = GetSelectedRoad();
	if (SelectedRoad != nullptr)
	{
		for (int32 Index = 0; Index < SelectedRoad->RoadPoints.Num(); ++Index)
		{
			Collector.ConsiderPoint(SelectedRoad->GetPos(Index), SelectedRoad, Index);
		}
	}

	const FRoadRayHit Hit = Collector.Resolve();
	if (Hit.bHit)
	{
		OutRoad = Cast<ARoadActor>(Hit.Owner);
		OutPointIndex = Hit.Index;
	}
}

void URoadTool_RoadPlan::SplitRoadAt(ARoadActor* Road, const FVector& Position)
{
	if (Road == nullptr)
	{
		return;
	}

	// Splitting a segment needs a segment. A road that is still a single point - the state a freshly
	// created road is in - has none, and asking the model for one would index past the array.
	if (Road->RoadPoints.Num() < 2)
	{
		return;
	}

	// A right click on a road also makes it the road being worked on, which is what the legacy tool did by
	// assigning the hit proxy's road into the mode's selection.
	SetSelectedRoad(Road);

	const FVector2D UV = Road->GetUV(Position);

	// DIAGNOSTIC (remove when the click path is confirmed).
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] SplitRoadAt points=%d uv=(%.1f,%.1f)"),
		Road->RoadPoints.Num(), UV.X, UV.Y);

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Road, GetRoadPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	// AddPoint() splits the segment the station falls in and answers with the index the new point was put
	// after. The legacy tool ignored that answer - the selection stays where it was - so the call is made
	// for its effect only.
	Road->AddPoint(UV.X);

	Change->CaptureAfter();
	EmitArrayChange(Road, MoveTemp(Change), LOCTEXT("AddRoadPlanPoint", "Add Road Plan Point"));

	Road->UpdateCurve();
	SyncProperties();
	RequestRebuild();
}

void URoadTool_RoadPlan::CreateRoadAt(const FVector& Position)
{
	USettings_RoadPlan* Data = GetMutableDefault<USettings_RoadPlan>();
	ARoadScene* Scene = GetRoadScene();
	if (Scene == nullptr)
	{
		// A road is an actor parented to the scene, so with no scene there is nothing to create one in.
		// The host normally guarantees one, which makes this a host bug rather than a user error - so say
		// so instead of letting the click look like it did nothing, which is what made this hard to spot.
		//
		// DIAGNOSTIC (remove when the click path is confirmed): the same news through UE_LOG as well,
		// because this project runs with screen messages suppressed, where DisplayMessage() is invisible.
		UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] CreateRoadAt ABORTED: no road scene"));
		if (UInteractiveToolManager* Manager = GetToolManager())
		{
			Manager->DisplayMessage(
				LOCTEXT("RoadPlanNoScene", "Road Plan: this level has no road scene, so a road cannot be created."),
				EToolMessageLevel::UserWarning);
		}
		return;
	}

	if (Data == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] CreateRoadAt ABORTED: no settings object"));
		return;
	}

	// DIAGNOSTIC (remove when the click path is confirmed).
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] CreateRoadAt pos=(%.0f,%.0f,%.0f) baseHeight=%.0f"),
		Position.X, Position.Y, Position.Z, Data->BaseHeight);

	ARoadActor* Road = nullptr;

	// AddRoad() spawns an actor, so no single-object change describes it. The scene is modified too - it
	// owns the road list - and the host transaction records both.
	//
	// The first point goes in inside the same bracket, because one click is one edit. InsertPointAt()
	// emits a change of its own, and a change emitted while a transaction is open joins it rather than
	// starting a second one (the host's FScopedTransaction is reference counted), so undoing the click
	// takes the road and its point away together. Undone separately they would leave a road with no
	// point on screen in between, which is a state the user never created and never asked to see.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("RoadPlan", "Road Plan"));
		Scene->Modify();
		Road = Scene->AddRoad(Data->Style.LoadSynchronous(), Data->BaseHeight);

		// Only once there is a road: a failed spawn must not leave an empty undo entry behind.
		if (Road != nullptr)
		{
			SetSelectedRoad(Road);
			PointIndex = INDEX_NONE;
			InsertPointAt(Road, Position);
		}
	}

	// DIAGNOSTIC (remove when the click path is confirmed).
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] AddRoad road=%d sceneRoads=%d"),
		Road != nullptr ? 1 : 0, Scene->Roads.Num());
}

void URoadTool_RoadPlan::InsertPointAt(ARoadActor* Road, const FVector& Position)
{
	if (Road == nullptr)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Road, GetRoadPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	// InsertPoint() answers through the index it was given: it appends when the caller pointed at the last
	// point (which is also the empty-curve case, where the index is INDEX_NONE) and otherwise inserts
	// before it, leaving the index naming the new point. PointIndex is a member for exactly that reason.
	//
	// The stored position is world XY rather than a station: an alignment point is where the user put it,
	// and the model derives the station from the fitted curve.
	Road->InsertPoint(FVector2D(Position.X, Position.Y), PointIndex);

	// DIAGNOSTIC (remove when the click path is confirmed): the last line of the create path, so its
	// arrival - and the point count - is the proof that a click really did produce a road.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] InsertPointAt points=%d"), Road->RoadPoints.Num());

	Change->CaptureAfter();
	EmitArrayChange(Road, MoveTemp(Change), LOCTEXT("AddRoadPlanPoint", "Add Road Plan Point"));

	Road->UpdateCurve();
	SelectRoadAndPoint(Road, PointIndex);
	RequestRebuild();
}

void URoadTool_RoadPlan::DeleteSelection()
{
	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return;
	}

	const int32 Index = GetPointIndex();
	if (Index != INDEX_NONE)
	{
		TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Road, GetRoadPointsProperty());
		if (Change == nullptr)
		{
			return;
		}

		// Removing an alignment point can disconnect the road at that end, which the model handles as part
		// of the curve refit below.
		Road->RoadPoints.RemoveAt(Index);

		Change->CaptureAfter();
		EmitArrayChange(Road, MoveTemp(Change), LOCTEXT("DeleteRoadPlanPoint", "Delete Road Plan Point"));

		PointIndex = INDEX_NONE;
		Road->UpdateCurve();
		SelectRoadAndPoint(Road, INDEX_NONE);
		RequestRebuild();
		return;
	}

	ARoadScene* Scene = GetRoadScene();
	if (Scene == nullptr)
	{
		return;
	}

	// With no point selected the Delete key drops the road itself, which destroys an actor and everything
	// hanging off it - so it goes through the host transaction like the other structural edits.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("RoadPlan", "Road Plan"));
		Scene->Modify();
		Scene->DestroyRoad(Road);
	}

	ResetSelection();
	RequestRebuild();
}

void URoadTool_RoadPlan::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;

	ARoadActor* HitRoad = nullptr;
	int32 HitPointIndex = INDEX_NONE;
	PickUnderRay(Ray, HitRoad, HitPointIndex);

	ARoadScene* Scene = GetRoadScene();
	ARoadActor* SelectedRoad = GetSelectedRoad();

	// DIAGNOSTIC (remove when the click path is confirmed): everything the branch below decides on, in
	// one line - so a click that picks nothing, a click with no scene, and a click that simply took the
	// wrong branch are told apart without another round trip.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] click btn=%s ray=(%.0f,%.0f,%.0f)->(%.2f,%.2f,%.2f) scene=%d roads=%d hitRoad=%d hitPoint=%d selected=%d"),
		bRightButton ? TEXT("R") : TEXT("L"),
		Ray.Origin.X, Ray.Origin.Y, Ray.Origin.Z,
		Ray.Direction.X, Ray.Direction.Y, Ray.Direction.Z,
		Scene != nullptr ? 1 : 0,
		Scene != nullptr ? Scene->Roads.Num() : -1,
		HitRoad != nullptr ? 1 : 0,
		HitPointIndex,
		SelectedRoad != nullptr ? 1 : 0);

	if (!bRightButton)
	{
		// Every road is always on screen in this tool, so a left click is a complete re-pick: a point, or a
		// road, or - when it hits neither - nothing at all.
		if (HitRoad != nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] branch=select"));
			SelectRoadAndPoint(HitRoad, HitPointIndex);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] branch=reset"));
			ResetSelection();
		}
		return;
	}

	const FVector Position = GetPlanPosition(Ray);

	// A right click on a road splits its alignment there; on empty space it extends the road being worked
	// on, or starts a new one when there is none.
	if (HitRoad != nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] branch=split pos=(%.0f,%.0f,%.0f)"),
			Position.X, Position.Y, Position.Z);
		SplitRoadAt(HitRoad, Position);
		return;
	}

	if (SelectedRoad == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] branch=create pos=(%.0f,%.0f,%.0f)"),
			Position.X, Position.Y, Position.Z);
		CreateRoadAt(Position);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [4 plan] branch=insert pos=(%.0f,%.0f,%.0f) points=%d"),
		Position.X, Position.Y, Position.Z, SelectedRoad->RoadPoints.Num());
	InsertPointAt(SelectedRoad, Position);
}

void URoadTool_RoadPlan::SelectParent()
{
	// The tool owns two levels of its own (road, then point), so the point goes first and the base then
	// steps the road up to its owning junction. ResetSelection() clears the point without touching the
	// road selection, which is what keeps the two steps independent.
	ResetSelection();
	Super::SelectParent();
}

FArrayProperty* URoadTool_RoadPlan::GetRoadPointsProperty()
{
	return FindFProperty<FArrayProperty>(ARoadActor::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ARoadActor, RoadPoints));
}

FInputRayHit URoadTool_RoadPlan::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Nothing to grab without a selected point, and nothing to measure against without a scale. The scale
	// is the gizmo's own: it is measured in Render() every frame, which is the frame the user aimed at.
	ARoadActor* Road = GetSelectedRoad();
	const int32 Index = GetPointIndex();
	if (Gizmo == nullptr || Road == nullptr || Index == INDEX_NONE || Gizmo->GetLastPixelToWorld() <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, Gizmo->GetLastPixelToWorld());

	// DIAGNOSTIC (remove when the drag path is confirmed): the handle test's own verdict, with the handle
	// it chose, so a press that fails to start a drag says whether it found no handle or found one and was
	// then overruled somewhere downstream.
	UE_LOG(LogTemp, Warning, TEXT("ROADINPUT [9 drag] plan handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes below record nothing themselves, so the stack gets a single entry
	// and undo puts the point back where the drag picked it up.
	DragChange = FRoadArrayChange::CaptureBefore(Road, GetRoadPointsProperty());

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit. The depth is the handle's
	// own pixel distance, which keeps this behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_RoadPlan::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	if (DraggedHandle == ERoadGizmoHandle::None || Gizmo == nullptr)
	{
		return;
	}

	// The gizmo measures the cursor's motion in the drag subspace it fixed at the press, and calls back
	// into ApplyPointTransform, exactly as its own drag would have - so the curve refit, the connection
	// search and the deferred rebuild are all the code that was already here.
	Gizmo->DragHandle(DraggedHandle, DragPos.WorldRay);
}

void URoadTool_RoadPlan::OnRoadDragEnded()
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
		ARoadActor* Road = GetSelectedRoad();
		if (Road != nullptr)
		{
			DragChange->CaptureAfter();
			EmitArrayChange(Road, MoveTemp(DragChange),
				LOCTEXT("MoveRoadPlanPoint", "Move Road Plan Point"));
		}
		DragChange.Reset();
	}
}

void URoadTool_RoadPlan::OnTick(float DeltaTime)
{
	UInteractiveTool::OnTick(DeltaTime);

	// DIAGNOSTIC (remove when the drag path is confirmed): the per-frame half of the gizmo investigation.
	// Every earlier probe fired on click or on Update(), so it could only ever describe the moment the
	// selection changed; the question left open was whether the gizmo actor survives the renderer at all
	// once the frame is drawn. This reads the two facts those probes never did:
	//
	//   editorHidden=1 -> the actor carries AActor::bIsTemporarilyHiddenInEditor, which is a second,
	//                     editor-only visibility flag that IsHidden() does NOT report. SetVisibility(true)
	//                     clears it, so a 1 here means something hid the actor again after the fact.
	//   rendered=0     -> WasRecentlyRendered() says no view drew the actor in the last second, which
	//                     separates "the handle is drawn but too small / culled" from "nothing draws the
	//                     actor at all" - the fork that decides whether the fault is in the geometry or in
	//                     the world the actor lives in.
	//
	// Throttled to about once a second so a per-frame hook does not bury the log.
	static double NextGizmoProbeTime = 0.0;
	const double Now = FPlatformTime::Seconds();
	if (Gizmo == nullptr || Now < NextGizmoProbeTime)
	{
		return;
	}
	NextGizmoProbeTime = Now + 1.0;

	bool bEditorHidden = false;
	bool bRecentlyRendered = false;
	const bool bHasActor = Gizmo->GetGizmoActorDiagnostics(bEditorHidden, bRecentlyRendered);
	UE_LOG(LogTemp, Warning,
		TEXT("ROADINPUT [7 tick] gizmo actor=%d editorHidden=%d rendered=%d handles=%d point=%d | %s"),
		bHasActor ? 1 : 0, bEditorHidden ? 1 : 0, bRecentlyRendered ? 1 : 0,
		Gizmo->GetGizmoHandleMask(), PointIndex, *Gizmo->GetGizmoWorldDiagnostics());
}

void URoadTool_RoadPlan::Render(IToolsContextRenderAPI* RenderAPI)
{
	UInteractiveTool::Render(RenderAPI);

	FPrimitiveDrawInterface* PDI = (RenderAPI != nullptr) ? RenderAPI->GetPrimitiveDrawInterface() : nullptr;
	if (PDI == nullptr)
	{
		return;
	}

	// The handles are drawn by the gizmo itself, in immediate mode, because its actor-based visuals are
	// never reached by this tool's scene pass. See URoadPointGizmo::Render for the full explanation.
	// The pixel scale the press-time hit test uses is cached inside the gizmo by the same call.
	if (Gizmo != nullptr)
	{
		Gizmo->Render(PDI, RenderAPI->GetSceneView());
	}

	// Every road is drawn, selected or not, because a left click can pick any of them.
	DrawRoads(PDI);

	ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr)
	{
		return;
	}

	const int32 SelectedPoint = GetPointIndex();
	for (int32 Index = 0; Index < Road->RoadPoints.Num(); ++Index)
	{
		PDI->DrawPoint(Road->GetPos(Index),
			(Index == SelectedPoint) ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Road,
			RoadToolStyle::Size_Point, SDPG_Foreground);
	}
}


#undef LOCTEXT_NAMESPACE
