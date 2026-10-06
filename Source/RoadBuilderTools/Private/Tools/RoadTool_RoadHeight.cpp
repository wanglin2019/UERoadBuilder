// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_RoadHeight.h"

#include "InputCoreTypes.h"
#include "InteractiveToolManager.h"
#include "RoadBuilderTools.h"
#include "RoadLog.h"
#include "SceneManagement.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadKeyInputBehavior.h"
#include "Tools/RoadPicking.h"
#include "Tools/RoadPointGizmo.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "RoadTool_RoadHeight"

UInteractiveTool* URoadTool_RoadHeightBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_RoadHeight* NewTool = NewObject<URoadTool_RoadHeight>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_RoadHeight::Setup()
{
	UInteractiveTool::Setup();

	// Click handling comes from ITF rather than a mode-level InputKey handler. Both buttons are bound:
	// the legacy tool selected with the left button and inserted a height point with the right one.
	AddClickBehavior();

	// The drag handle binding, which is what makes the axis handle grabbable; it declines every press
	// that did not land on one, so clicking to select is untouched.
	AddDragBehavior();

	// Keyboard needs a behaviour of our own: ITF only ships keyboard behaviours for modifier keys.
	DeleteKeyBehavior = NewObject<URoadKeyInputBehavior>();
	DeleteKeyBehavior->Initialize(EKeys::Delete, [this]() { DeleteSelectedPoint(); });
	AddInputBehavior(DeleteKeyBehavior);

	Properties = NewObject<URoadTool_RoadHeightProperties>(this, TEXT("RoadHeightSettings"));
	AddToolPropertySource(Properties);

	// Created here and left hidden until a point is selected, so the tool is usable before the first click.
	// The two callbacks are the halves that are specific to a height point; everything else about the drag
	// - the proxy, the gizmo, the deferred rebuild bracket - belongs to the shared helper.
	Gizmo = NewObject<URoadPointGizmo>(this);
	Gizmo->Initialize(this, ETransformGizmoSubElements::TranslateAxisX,
		[this]() { return GetPointTransform(); },
		[this](const FTransform& NewTransform) { ApplyPointTransform(NewTransform); },
		[this]() { RebuildAfterDrag(); });

	// A road may already be selected when the tool starts; show its profile either way.
	SelectPoint(GetSelectedRoad(), INDEX_NONE);
}

void URoadTool_RoadHeight::Shutdown(EToolShutdownType ShutdownType)
{
	if (Gizmo != nullptr)
	{
		Gizmo->Shutdown();
	}

	UInteractiveTool::Shutdown(ShutdownType);
}

void URoadTool_RoadHeight::Render(IToolsContextRenderAPI* RenderAPI)
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

	DrawRoads(PDI);

	const ARoadActor* Road = GetSelectedRoad();
	if (Road == nullptr || Road->BaseCurve == nullptr)
	{
		return;
	}

	const int32 SelectedIndex = GetSelectedPointIndex();
	for (int32 PointIndex = 0; PointIndex < Road->HeightPoints.Num(); ++PointIndex)
	{
		const FVector Position = Road->BaseCurve->GetPos(Road->HeightPoints[PointIndex].Dist);
		const bool bSelected = (PointIndex == SelectedIndex);
		PDI->DrawPoint(Position, bSelected ? RoadToolStyle::Color_Select : RoadToolStyle::Color_Road,
			RoadToolStyle::Size_Point, SDPG_Foreground);
	}
}

void URoadTool_RoadHeight::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	UInteractiveTool::OnPropertyModified(PropertySet, Property);

	if (PropertySet != Properties || Property == nullptr)
	{
		return;
	}

	// PointIndex is the tool's own bookkeeping, not something the user edits.
	if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(URoadTool_RoadHeightProperties, PointIndex))
	{
		return;
	}

	ApplyPropertiesToPoint();
}

void URoadTool_RoadHeight::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;
	ARoadActor* Road = GetSelectedRoad();

	if (bRightButton)
	{
		// Right click inserts a height point where the cursor meets the level - the legacy gesture that
		// turns "I want a break in the profile here" into a single click.
		if (Road == nullptr)
		{
			return;
		}

		const FVector Position = LineTrace(Ray, Road);
		if (Position.X >= WORLD_MAX)
		{
			// The legacy tool projected the sentinel onto the road, which produced an arbitrary distance.
			// A missed trace now simply does nothing.
			return;
		}
		AddHeightPoint(Road, Road->GetUV(Position).X);
		return;
	}

	const int32 PointIndex = PickHeightPoint(Road, Ray);
	if (PointIndex != INDEX_NONE)
	{
		SelectPoint(Road, PointIndex);
		return;
	}

	// Nothing of the selected road was hit, so the click was aimed at a road instead. The legacy tools
	// got this from proxies drawn during Render(); ITF has no proxies, so the ray test happens here -
	// same policy, different mechanism.
	SelectPoint(SelectRoadUnderRay(Ray), INDEX_NONE);
}

void URoadTool_RoadHeight::SelectParent()
{
	SelectPoint(nullptr, INDEX_NONE);
	Super::SelectParent();
}

void URoadTool_RoadHeight::SelectPoint(ARoadActor* Road, int32 PointIndex)
{
	Properties->PointIndex = (Road != nullptr && Road->HeightPoints.IsValidIndex(PointIndex)) ? PointIndex : INDEX_NONE;

	SyncPropertiesFromPoint();

	// Shows the gizmo on the new point, or hides it when the selection is empty.
	Gizmo->Update(Properties->PointIndex != INDEX_NONE);
	RequestRebuild();
}

int32 URoadTool_RoadHeight::GetSelectedPointIndex() const
{
	if (Properties == nullptr)
	{
		return INDEX_NONE;
	}

	const ARoadActor* Road = GetSelectedRoad();
	const int32 PointIndex = Properties->PointIndex;
	return (Road != nullptr && Road->HeightPoints.IsValidIndex(PointIndex)) ? PointIndex : INDEX_NONE;
}

void URoadTool_RoadHeight::SyncPropertiesFromPoint()
{
	const ARoadActor* Road = GetSelectedRoad();
	const int32 PointIndex = GetSelectedPointIndex();
	if (Road == nullptr || PointIndex == INDEX_NONE)
	{
		Properties->PointIndex = INDEX_NONE;
		Properties->Dist = 0.0;
		Properties->Height = 0.0;
		Properties->Range = 3000.0;
		return;
	}

	const FHeightPoint& Point = Road->HeightPoints[PointIndex];
	Properties->Dist = Point.Dist;
	Properties->Height = Point.Height;
	Properties->Range = Point.Range;
}

FTransform URoadTool_RoadHeight::GetPointTransform() const
{
	const ARoadActor* Road = GetSelectedRoad();
	const int32 PointIndex = GetSelectedPointIndex();
	if (Road == nullptr || Road->BaseCurve == nullptr || PointIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	const double Dist = Road->HeightPoints[PointIndex].Dist;

	// The legacy tool reported the road tangent from GetCustomDrawingCoordinateSystem(), which is what
	// restricted the editor's move widget to "along the road". The same intent is expressed here as the
	// gizmo's own rotation, so the single translate axis it exposes points down the road.
	const FVector Direction = Road->BaseCurve->GetDir(Dist);
	return FTransform(Direction.Rotation(), Road->BaseCurve->GetPos(Dist));
}

void URoadTool_RoadHeight::ApplyPointTransform(const FTransform& NewTransform)
{
	ARoadActor* Road = GetSelectedRoad();
	const int32 PointIndex = GetSelectedPointIndex();
	if (Road == nullptr || PointIndex == INDEX_NONE)
	{
		return;
	}

	FHeightPoint& Point = Road->HeightPoints[PointIndex];

	// Project the requested position back onto the road rather than accumulating a drag delta. This is
	// idempotent, so a gizmo transform that lags behind the clamped point cannot drift - which is the
	// job the editor used to do for the legacy tool by re-querying GetWidgetLocation() every frame.
	const FVector2D UV = Road->GetUV(NewTransform.GetLocation());
	const double NewDist = FMath::Clamp(UV.X, 0.0, Road->Length());
	if (FMath::IsNearlyEqual(NewDist, Point.Dist))
	{
		return;
	}

	Point.Dist = NewDist;

	// No undo entry here: a self-managed drag brackets the whole gesture with one FRoadArrayChange
	// (snapshot on the press in CanBeginRoadDrag, emission on the release in OnRoadDragEnded), whose
	// replay writes the restored array straight back. The proxy's own change source never fires in this
	// host - gizmo-driven input is the path that is dead - so there is nothing to double up with.
	Road->UpdateCurve();
	SyncPropertiesFromPoint();

	// Mid-drag the viewport is already being redrawn by the gizmo, and rebuilding road geometry every
	// frame would be wasted work; the drag's end event issues the rebuild instead.
	if (!Gizmo->IsDragging())
	{
		RequestRebuild();
	}
}

void URoadTool_RoadHeight::RebuildAfterDrag()
{
	RequestRebuild();
}

FInputRayHit URoadTool_RoadHeight::CanBeginRoadDrag(const FInputDeviceRay& PressPos)
{
	// Nothing to grab without a selected height point, and nothing to measure against without a scale.
	// The scale is the gizmo's own: it is measured in Render() every frame, the frame the user aimed at.
	ARoadActor* Road = GetSelectedRoad();
	const int32 PointIndex = GetSelectedPointIndex();
	const double PixelToWorld = (Gizmo != nullptr) ? Gizmo->GetLastPixelToWorld() : 0.0;
	if (Gizmo == nullptr || Road == nullptr || PointIndex == INDEX_NONE || PixelToWorld <= 0.0)
	{
		return FInputRayHit();
	}

	const FRoadGizmoHit Hit = Gizmo->HitTestHandle(PressPos.WorldRay, PixelToWorld);

	// The handle test's own verdict, per tool, so a press that fails to start a drag says whether it found
	// no handle or was overruled downstream. Verbose: one line per press, not per move.
	RoadLog_Debug(TEXT("drag roadheight handle=%d pixel=%.1f"),
		static_cast<int32>(Hit.Handle), Hit.PixelDistance);

	if (!Hit.bHit)
	{
		return FInputRayHit();
	}

	// One undo step for the whole drag: the snapshot is taken now, on the press, and emitted on the
	// release - the per-frame writes in ApplyPointTransform record nothing themselves.
	DragChange = FRoadArrayChange::CaptureBefore(Road, GetHeightPointsProperty());

	DraggedHandle = Hit.Handle;
	Gizmo->BeginDrag(DraggedHandle, PressPos.WorldRay);

	// The grabbed handle is what the drag is about, so report it as the hit; the depth keeps this
	// behaviour ahead of the plain click behaviour on the tie-break.
	return FInputRayHit(static_cast<float>(Hit.PixelDistance));
}

void URoadTool_RoadHeight::OnRoadDragged(const FInputDeviceRay& DragPos)
{
	if (DraggedHandle == ERoadGizmoHandle::None || Gizmo == nullptr)
	{
		return;
	}

	// The gizmo measures the cursor's motion in the subspace it fixed at the press and calls back into
	// ApplyPointTransform, so the existing write path - the projection onto the road, the clamp and the
	// deferred rebuild - is all reused.
	Gizmo->DragHandle(DraggedHandle, DragPos.WorldRay);
}

void URoadTool_RoadHeight::OnRoadDragEnded()
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
		if (ARoadActor* Road = GetSelectedRoad())
		{
			DragChange->CaptureAfter();
			EmitArrayChange(Road, MoveTemp(DragChange), LOCTEXT("MoveHeightPoint", "Move Road Height Point"));
		}
		DragChange.Reset();
	}
}

void URoadTool_RoadHeight::ApplyPropertiesToPoint()
{
	ARoadActor* Road = GetSelectedRoad();
	const int32 PointIndex = GetSelectedPointIndex();
	if (Road == nullptr || PointIndex == INDEX_NONE)
	{
		return;
	}

	FHeightPoint& Point = Road->HeightPoints[PointIndex];
	const double NewDist = FMath::Clamp(Properties->Dist, 0.0, Road->Length());
	if (FMath::IsNearlyEqual(NewDist, Point.Dist)
		&& FMath::IsNearlyEqual(Properties->Height, Point.Height)
		&& FMath::IsNearlyEqual(Properties->Range, Point.Range))
	{
		// The details panel reports every edit it forwards; only a real change earns an undo entry.
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Road, GetHeightPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	Point.Dist = NewDist;
	Point.Height = Properties->Height;
	Point.Range = Properties->Range;
	Change->CaptureAfter();

	EmitArrayChange(Road, MoveTemp(Change), LOCTEXT("EditHeightPoint", "Edit Road Height Point"));

	Road->UpdateCurve();
	Gizmo->Update(GetSelectedPointIndex() != INDEX_NONE);
	RequestRebuild();
}

void URoadTool_RoadHeight::AddHeightPoint(ARoadActor* Road, double Dist)
{
	// AddHeight() indexes a pair of existing points, so a road with no profile to split cannot take one.
	if (Road == nullptr || Road->HeightPoints.Num() < 2)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Road, GetHeightPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	const double NewDist = FMath::Clamp(Dist, 0.0, Road->Length());
	// +1: AddHeight() returns the index of the point before the new one; see the header note.
	const int32 NewIndex = Road->AddHeight(NewDist) + 1;
	Change->CaptureAfter();

	EmitArrayChange(Road, MoveTemp(Change), LOCTEXT("EditHeightPoint", "Edit Road Height Point"));

	Road->UpdateCurve();
	SelectPoint(Road, NewIndex);
}

void URoadTool_RoadHeight::DeleteSelectedPoint()
{
	ARoadActor* Road = GetSelectedRoad();
	const int32 PointIndex = GetSelectedPointIndex();
	if (Road == nullptr || PointIndex == INDEX_NONE)
	{
		return;
	}

	TUniquePtr<FRoadArrayChange> Change = FRoadArrayChange::CaptureBefore(Road, GetHeightPointsProperty());
	if (Change == nullptr)
	{
		return;
	}

	Road->HeightPoints.RemoveAt(PointIndex);
	Change->CaptureAfter();

	EmitArrayChange(Road, MoveTemp(Change), LOCTEXT("EditHeightPoint", "Edit Road Height Point"));

	Road->UpdateCurve();
	SelectPoint(Road, INDEX_NONE);
}

FArrayProperty* URoadTool_RoadHeight::GetHeightPointsProperty()
{
	return FindFProperty<FArrayProperty>(ARoadActor::StaticClass(),
		GET_MEMBER_NAME_CHECKED(ARoadActor, HeightPoints));
}

int32 URoadTool_RoadHeight::PickHeightPoint(const ARoadActor* Road, const FRay& Ray) const
{
	if (Road == nullptr || Road->BaseCurve == nullptr)
	{
		return INDEX_NONE;
	}

	FRoadHitCollector Collector(Ray);
	for (int32 PointIndex = 0; PointIndex < Road->HeightPoints.Num(); ++PointIndex)
	{
		Collector.ConsiderPoint(Road->BaseCurve->GetPos(Road->HeightPoints[PointIndex].Dist), nullptr, PointIndex);
	}

	const FRoadRayHit Hit = Collector.Resolve();
	return Hit.bHit ? Hit.Index : INDEX_NONE;
}

#undef LOCTEXT_NAMESPACE
