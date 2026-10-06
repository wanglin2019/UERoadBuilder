// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_GroundEdit.h"

#include "GroundActor.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "RoadScene.h"
#include "SceneManagement.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPointGizmo.h"
#include "Tools/RoadPropertyChange.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_GroundEdit"

UInteractiveTool* URoadTool_GroundEditBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_GroundEdit* NewTool = NewObject<URoadTool_GroundEdit>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_GroundEdit::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The drag handle binding, which is what makes the translated handles grabbable; it declines every
	// press that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	Properties = NewObject<URoadTool_GroundEditProperties>(this, TEXT("GroundEditSettings"));
	AddToolPropertySource(Properties);

	// No keyboard binding: the legacy tool had none either, because a ground point is removed by editing
	// the outline it belongs to, not by selecting and deleting it.
	//
	// Plane + both axes: the ground-flush XY rectangle self-culls from any camera below ~15 degrees of
	// elevation, so the two axis arrows are what actually stays grabbable. See URoadTool_RoadPlan::Setup
	// for the full note on UGizmoRectangleComponent's view-dependent render cull.
	Gizmo = NewObject<URoadPointGizmo>(this);
	Gizmo->Initialize(this,
		ETransformGizmoSubElements::TranslatePlaneXY |
		ETransformGizmoSubElements::TranslateAxisX |
		ETransformGizmoSubElements::TranslateAxisY,
		[this]() { return GetPointTransform(); },
		[this](const FTransform& NewTransform) { ApplyPointTransform(NewTransform); },
		[this]() { RebuildAfterDrag(); });

	// A ground may already be selected when the tool starts; pick the selection up rather than ignoring it.
	// When there is none, the selection is left untouched - the host shares one actor selection between
	// grounds, roads and junctions, so writing "no ground" here would also drop a selected road.
	if (AGroundActor* ExistingGround = GetCurrentGround())
	{
		SelectPoint(ExistingGround, INDEX_NONE);
	}
	else
	{
		SyncProperties();
	}
}

void URoadTool_GroundEdit::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

AGroundActor* URoadTool_GroundEdit::GetCurrentGround() const
{
	AGroundActor* Ground = GetSelectedGround();
	ARoadScene* Scene = GetRoadScene();
	if (Ground == nullptr || Scene == nullptr)
	{
		return nullptr;
	}

	// A rebuild or an undo can leave a stale selection behind, so it is only trusted while the scene still
	// lists the ground - the same test the legacy tool effectively got for free from actor lifetime.
	return Scene->Grounds.Contains(Ground) ? Ground : nullptr;
}

int32 URoadTool_GroundEdit::GetPointIndex() const
{
	AGroundActor* Ground = GetCurrentGround();
	return (Ground != nullptr && Ground->Points.IsValidIndex(PointIndex)) ? PointIndex : INDEX_NONE;
}

int32 URoadTool_GroundEdit::GetManualPointIndex() const
{
	AGroundActor* Ground = GetCurrentGround();
	const int32 Index = GetPointIndex();
	if (Ground == nullptr || Index == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// A point is free exactly when it names no road; its Index then addresses ManualPoints.
	const FGroundPoint& Point = Ground->Points[Index];
	if (Point.Road != nullptr || !Ground->ManualPoints.IsValidIndex(Point.Index))
	{
		return INDEX_NONE;
	}

	return Point.Index;
}

void URoadTool_GroundEdit::SelectPoint(AGroundActor* Ground, int32 Index)
{
	// A ground with no point selected is a legitimate state: clicking the outline picks the area up so its
	// endpoint can then be used as the anchor of a join or a close.
	PointIndex = (Ground != nullptr && Ground->Points.IsValidIndex(Index)) ? Index : INDEX_NONE;
	SetSelectedGround(Ground);

	SyncProperties();
	Gizmo->Update(GetManualPointIndex() != INDEX_NONE);
	RequestRedraw();
}

void URoadTool_GroundEdit::SyncProperties()
{
	AGroundActor* Ground = GetCurrentGround();
	const int32 Index = GetPointIndex();
	if (Ground == nullptr || Index == INDEX_NONE)
	{
		Properties->PointIndex = INDEX_NONE;
		Properties->bManualPoint = false;
		Properties->RoadPointRoad = nullptr;
		Properties->RoadPointSide = 0;
		Properties->RoadPointIndex = 0;
		Properties->ManualPoint = FVector::ZeroVector;
		return;
	}

	const FGroundPoint& Point = Ground->Points[Index];
	const int32 ManualIndex = GetManualPointIndex();
	Properties->PointIndex = Index;
	Properties->bManualPoint = (ManualIndex != INDEX_NONE);
	Properties->RoadPointRoad = Point.Road;
	Properties->RoadPointSide = Point.Side;
	Properties->RoadPointIndex = Point.Index;
	Properties->ManualPoint = (ManualIndex != INDEX_NONE) ? Ground->ManualPoints[ManualIndex] : FVector::ZeroVector;
}

FTransform URoadTool_GroundEdit::GetPointTransform() const
{
	AGroundActor* Ground = GetCurrentGround();
	const int32 ManualIndex = GetManualPointIndex();
	if (Ground == nullptr || ManualIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	// Identity rotation: the legacy tool never overrode GetCustomDrawingCoordinateSystem, so its widget
	// stayed on the world axes and a drag arrived in world space.
	return FTransform(Ground->ManualPoints[ManualIndex]);
}

void URoadTool_GroundEdit::ApplyPointTransform(const FTransform& NewTransform)
{
	AGroundActor* Ground = GetCurrentGround();
	const int32 ManualIndex = GetManualPointIndex();
	if (Ground == nullptr || ManualIndex == INDEX_NONE)
	{
		return;
	}

	FVector& ManualPoint = Ground->ManualPoints[ManualIndex];

	// The gizmo reports an absolute position, so the delta is measured against the data: re-seating the
	// gizmo is then a no-op and the point cannot drift away from the cursor.
	const FVector Delta = NewTransform.GetLocation() - ManualPoint;
	if (Delta.IsNearlyZero())
	{
		return;
	}

	ManualPoint += Delta;

	// No explicit undo entry: the gizmo's own change replays this edit through the proxy.
	SyncProperties();

	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_GroundEdit::RebuildAfterDrag()
{
	RequestRebuild();
}

FInputRayHit URoadTool_GroundEdit::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Only a free point has a gizmo to grab, and nothing to measure against without a scale. The scale is
	// the gizmo's own: it is measured in Render() every frame, which is the frame the user aimed at.
	AGroundActor* Ground = GetCurrentGround();
	const int32 ManualIndex = GetManualPointIndex();
	const double PixelToWorld = (Gizmo != nullptr) ? Gizmo->GetLastPixelToWorld() : 0.0;
	if (Gizmo == nullptr || Ground == nullptr || ManualIndex == INDEX_NONE || PixelToWorld <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, PixelToWorld);

	// The handle test's own verdict, per tool, so a press that fails to start a drag says whether it found
	// no handle or was overruled downstream. Verbose: one line per press, not per move.
	RoadLog_Debug(TEXT("drag ground handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes in ApplyPointTransform record nothing themselves.
	DragChange = FRoadArrayChange::CaptureBefore(Ground, GetManualPointsProperty());

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit; the depth keeps this
	// behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_GroundEdit::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	if (DraggedHandle == ERoadGizmoHandle::None || Gizmo == nullptr)
	{
		return;
	}

	// The gizmo measures the cursor's motion in the subspace it fixed at the press and calls back into
	// ApplyPointTransform, so the existing write path - the deferred rebuild included - is all reused.
	Gizmo->DragHandle(DraggedHandle, DragPos.WorldRay);
}

void URoadTool_GroundEdit::OnRoadDragEnded()
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
		if (AGroundActor* Ground = GetCurrentGround())
		{
			DragChange->CaptureAfter();
			EmitArrayChange(Ground, MoveTemp(DragChange), LOCTEXT("MoveGroundPoint", "Move Ground Point"));
		}
		DragChange.Reset();
	}
}

void URoadTool_GroundEdit::ApplyProperties(FProperty* Property)
{
	AGroundActor* Ground = GetCurrentGround();
	const int32 ManualIndex = GetManualPointIndex();
	if (Ground == nullptr || ManualIndex == INDEX_NONE || Property == nullptr)
	{
		return;
	}

	// Only the free position is editable - the road a point follows and where along it are properties of
	// the road, not of the ground - so an edit to anything else is ignored rather than misapplied.
	if (Property->GetFName() != GET_MEMBER_NAME_CHECKED(URoadTool_GroundEditProperties, ManualPoint))
	{
		return;
	}

	// ManualPoints is an array of structs, so the same array record the other tools use covers an element
	// of it: the record's whole-array snapshot does not care how many elements there are.
	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Ground, GetManualPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	Ground->ManualPoints[ManualIndex] = Properties->ManualPoint;

	Change->CaptureAfter();
	EmitArrayChange(Ground, MoveTemp(Change), LOCTEXT("EditGroundPoint", "Edit Ground Point"));

	RequestRebuild();
}

void URoadTool_GroundEdit::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	ApplyProperties(Property);
}

FVector URoadTool_GroundEdit::GetPointPosition(AGroundActor* Ground, int32 Index) const
{
	if (Ground == nullptr || !Ground->Points.IsValidIndex(Index))
	{
		return FVector::ZeroVector;
	}

	// Non-const on purpose: resolving a station to a world position is not a const operation in the model.
	FGroundPoint& Point = Ground->Points[Index];
	if (Point.Road != nullptr)
	{
		// A point that follows a road needs the junction slots to know where along that road it sits, so
		// the scene is asked the same question the legacy drawing and picking passes asked.
		ARoadScene* Scene = GetRoadScene();
		if (Scene == nullptr)
		{
			return FVector::ZeroVector;
		}

		TMap<ARoadActor*, TArray<FJunctionSlot>> RoadSlots = Scene->GetAllJunctionSlots();
		return Point.GetPos(RoadSlots);
	}

	return Ground->ManualPoints.IsValidIndex(Point.Index) ? Ground->ManualPoints[Point.Index] : FVector::ZeroVector;
}

void URoadTool_GroundEdit::PickUnderRay(const FRay& Ray, AGroundActor*& OutGround, int32& OutIndex) const
{
	OutGround = nullptr;
	OutIndex = INDEX_NONE;

	ARoadScene* Scene = GetRoadScene();
	if (Scene == nullptr)
	{
		return;
	}

	TMap<ARoadActor*, TArray<FJunctionSlot>> RoadSlots = Scene->GetAllJunctionSlots();

	FRoadHitCollector Collector(Ray);
	for (AGroundActor* Ground : Scene->Grounds)
	{
		if (Ground == nullptr)
		{
			continue;
		}

		// The outline itself, labelled with no index: what the legacy whole-ground hit proxy meant.
		const TArray<FVector> Vertices = Ground->GetVertices(RoadSlots);
		const int32 SegmentCount = Vertices.Num() - !Ground->bClosedLoop;
		for (int32 Vertex = 0; Vertex < SegmentCount; ++Vertex)
		{
			Collector.ConsiderSegment(Vertices[Vertex], Vertices[(Vertex + 1) % Vertices.Num()], Ground);
		}

		// The points, which the legacy pass drew with their own proxies on top of the outline.
		for (int32 Index = 0; Index < Ground->Points.Num(); ++Index)
		{
			Collector.ConsiderPoint(GetPointPosition(Ground, Index), Ground, Index);
		}
	}

	const FRoadRayHit Hit = Collector.Resolve();
	if (Hit.bHit)
	{
		OutGround = Cast<AGroundActor>(Hit.Owner);
		OutIndex = Hit.Index;
	}
}

void URoadTool_GroundEdit::CloseCurrentGround(AGroundActor* Ground)
{
	if (Ground == nullptr)
	{
		return;
	}

	// Closing the loop is a single flag on the ground, so it needs no transaction bracket of its own.
	TUniquePtr<FRoadPropertyChange> Change =
		FRoadPropertyChange::CaptureBefore(Ground, GetClosedLoopProperty());
	if (Change == nullptr)
	{
		return;
	}

	Ground->bClosedLoop = true;

	Change->CaptureAfter();
	EmitPropertyChange(Ground, MoveTemp(Change), LOCTEXT("CloseGround", "Close Ground"));

	SyncProperties();
	RequestRebuild();
}

void URoadTool_GroundEdit::JoinGroundAt(AGroundActor* Ground, AGroundActor* Other)
{
	ARoadScene* Scene = GetRoadScene();
	if (Ground == nullptr || Other == nullptr || Other == Ground || Scene == nullptr)
	{
		return;
	}

	// Joining moves every point of Other into Ground and then destroys Other, so no single-object change
	// can describe it. Both halves get their own record inside one bracket, which is also what makes the
	// whole gesture undo as a single step.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("GroundEdit", "Join Ground"));

		Ground->Modify();
		Other->Modify();
		Ground->Join(Other, PointIndex);

		Scene->Grounds.Remove(Other);
		Other->Destroy();
	}

	// The selection can have changed identity through the join - PointIndex was passed in by reference and
	// names the far end of the merged outline now - so the panel is refreshed from the new state.
	SyncProperties();
	RequestRebuild();
}

void URoadTool_GroundEdit::InsertManualPointAt(AGroundActor* Ground, const FRay& Ray)
{
	ARoadScene* Scene = GetRoadScene();
	const int32 Index = GetPointIndex();
	if (Ground == nullptr || Scene == nullptr || Index == INDEX_NONE)
	{
		return;
	}

	// The new point lands on the horizontal plane the selected point already sits in, which is what the
	// legacy tool did with the same plane-through-the-point construction.
	const FVector Reference = GetPointPosition(Ground, Index);
	const FVector Position = FMath::RayPlaneIntersection(Ray.Origin, Ray.Direction,
		FPlane(FVector(0.0, 0.0, Reference.Z), FVector::UpVector));

	// AddManualPoint() inserts into two arrays at once - the free positions and the outline - so the edit
	// is bracketed rather than recorded as one array's change.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("GroundEdit", "Add Ground Point"));
		Ground->Modify();
		Ground->AddManualPoint(Position, PointIndex);
	}

	SyncProperties();
	RequestRebuild();
}

void URoadTool_GroundEdit::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;
	AGroundActor* Ground = GetCurrentGround();

	AGroundActor* HitGround = nullptr;
	int32 HitIndex = INDEX_NONE;
	PickUnderRay(Ray, HitGround, HitIndex);

	if (!bRightButton)
	{
		// A left click that hits nothing deliberately leaves the selection alone, which is what the legacy
		// tool did here - unlike the marking tools, which cleared theirs.
		if (HitGround != nullptr)
		{
			SelectPoint(HitGround, HitIndex);
		}
		return;
	}

	// A right click only means something when the click started from an endpoint of the selected outline;
	// there is no other point to extend from.
	if (Ground == nullptr || !Ground->IsEndPoint(GetPointIndex()))
	{
		return;
	}

	if (HitGround != nullptr)
	{
		if (HitGround != Ground)
		{
			// A different ground: join the two, which requires the other one to be an endpoint as well.
			if (HitGround->IsEndPoint(HitIndex))
			{
				JoinGroundAt(Ground, HitGround);
			}
		}
		else if (HitIndex != PointIndex)
		{
			// The same ground's far endpoint: close the loop.
			if (HitGround->IsEndPoint(HitIndex))
			{
				CloseCurrentGround(Ground);
			}
		}
		return;
	}

	InsertManualPointAt(Ground, Ray);
}

void URoadTool_GroundEdit::SelectParent()
{
	// The ground half stays: this tool has no junction level above it, so the base only clears a road
	// selection it never made, and the point drop is the whole step.
	SelectPoint(nullptr, INDEX_NONE);
	Super::SelectParent();
}

FArrayProperty* URoadTool_GroundEdit::GetManualPointsProperty()
{
	return FindFProperty<FArrayProperty>(AGroundActor::StaticClass(),
		GET_MEMBER_NAME_CHECKED(AGroundActor, ManualPoints));
}

FBoolProperty* URoadTool_GroundEdit::GetClosedLoopProperty()
{
	return FindFProperty<FBoolProperty>(AGroundActor::StaticClass(),
		GET_MEMBER_NAME_CHECKED(AGroundActor, bClosedLoop));
}

void URoadTool_GroundEdit::Render(IToolsContextRenderAPI* RenderAPI)
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

	ARoadScene* Scene = GetRoadScene();
	if (Scene == nullptr)
	{
		return;
	}

	TMap<ARoadActor*, TArray<FJunctionSlot>> RoadSlots = Scene->GetAllJunctionSlots();
	AGroundActor* Selected = GetCurrentGround();
	const int32 SelectedIndex = GetPointIndex();

	for (AGroundActor* Ground : Scene->Grounds)
	{
		if (Ground == nullptr)
		{
			continue;
		}

		const FLinearColor Color = (Ground == Selected)
			? RoadToolStyle::Color_Select
			: RoadToolStyle::Color_Road;

		const TArray<FVector> Vertices = Ground->GetVertices(RoadSlots);
		for (int32 Vertex = 0; Vertex < Vertices.Num() - !Ground->bClosedLoop; ++Vertex)
		{
			PDI->DrawLine(Vertices[Vertex], Vertices[(Vertex + 1) % Vertices.Num()], Color, SDPG_Foreground,
				RoadToolStyle::Thickness_Road, 0.0f, true);
		}

		for (int32 Index = 0; Index < Ground->Points.Num(); ++Index)
		{
			const bool bSelectedPoint = (Ground == Selected) && (Index == SelectedIndex);
			PDI->DrawPoint(GetPointPosition(Ground, Index),
				bSelectedPoint ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Road,
				RoadToolStyle::Size_Point, SDPG_Foreground);
		}
	}
}

#undef LOCTEXT_NAMESPACE
