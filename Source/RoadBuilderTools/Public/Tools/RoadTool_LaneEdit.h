// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"

#include "RoadTool_LaneEdit.generated.h"

class URoadKeyInputBehavior;
class URoadLane;
class URoadPointGizmo;

/**
 * Settings of URoadTool_LaneEdit: a live mirror of the selected lane segment.
 *
 * The legacy tool pushed the raw FLaneSegment into a struct details view and reacted to edits through a
 * NotifyHook callback. Here the mirrored values are the UI, and the tool notices edits in
 * OnPropertyModified(). Nothing here knows whether an editor panel or a runtime widget is showing it.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_LaneEditProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Segment these values mirror; INDEX_NONE when none is selected. Owned by the tool, not the user. */
	UPROPERTY(VisibleAnywhere, Category = "Lane Edit")
	int32 SegmentIndex = INDEX_NONE;

	/** Station along the road where the segment starts. */
	UPROPERTY(EditAnywhere, Category = "Lane Edit", meta = (ClampMin = "0.0", UIMin = "0.0"))
	double Dist = 0.0;

	/** Cross-section of the lane over this segment. */
	UPROPERTY(EditAnywhere, Category = "Lane Edit")
	TObjectPtr<ULaneShape> LaneShape = nullptr;

	/** What the lane is for over this segment; setting it to Collapsed pins the lane shut. */
	UPROPERTY(EditAnywhere, Category = "Lane Edit")
	ELaneType LaneType = ELaneType::None;
};

/** Builder for URoadTool_LaneEdit. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_LaneEditBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Edits a lane's segments: where each one starts, its shape, and what kind of lane it is.
 *
 * Legacy gesture, preserved: left click on a road selects it, left click again picks the lane and segment
 * under the cursor, right click on one of the lane's own boundaries copies the lane across it (or, on the
 * lane itself, splits the current segment in two at the cursor).
 *
 * | legacy (FModeTool)                                    | here                                        |
 * |-------------------------------------------------------|---------------------------------------------|
 * | HitProxyCast<HRoadCurveProxy> for the lane's borders   | RoadPicking::PickBoundary() over the two    |
 * | GetWidgetLocation / GetCustomDrawingCoordinateSystem / InputDelta | URoadPointGizmo, rotated onto the road |
 * | ShowStruct(FLaneSegment) + NotifyHook                  | URoadTool_LaneEditProperties                |
 * | InputKey(Home/End/Tab/Delete)                          | four URoadKeyInputBehavior instances        |
 * | FScopedTransaction for CopyLane only                   | FRoadUndoTransaction for CopyLane only      |
 * | Render(PDI) + hit proxies                              | Render(RenderAPI), no hit proxies           |
 *
 * The Home/End/Tab/Delete walk is why this tool needed the keyboard behaviour: it was the only way to
 * step through a lane's segments without leaving the viewport.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_LaneEdit : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual void OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the segment selection, then let the base step the road selection up a level. */
	virtual void SelectParent() override;

	/**
	 * Whether the press landed on the gizmo's axis handle.
	 *
	 * The gizmo's own hit test is not driven by this host, so the handle test runs here, against the same
	 * geometry Render() draws; a report of no hit leaves the press an ordinary click. The single axis is
	 * the road tangent, so a drag moves the segment's start along the road.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_LaneEditProperties> Properties;

	UPROPERTY()
	TObjectPtr<URoadPointGizmo> Gizmo;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> HomeKeyBehavior;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> EndKeyBehavior;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> TabKeyBehavior;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> DeleteKeyBehavior;

private:
	/** The lane being edited, or null when the selection no longer resolves. */
	URoadLane* GetCurrentLane() const;

	/** Makes Lane/SegmentIndex the selection and refreshes the panel, the gizmo and the view. */
	void SelectSegment(URoadLane* Lane, int32 InSegmentIndex);

	/** Copies the selected segment into Properties, so the panel shows live values. */
	void SyncPropertiesFromSegment();

	/** Writes the panel back into the road, with one undo entry and the legacy per-field reactions. */
	void ApplyPropertiesToSegment(FProperty* Property);

	/** Where the gizmo sits: the segment start, rotated onto the road tangent. */
	FTransform GetSegmentTransform() const;

	/** Applies a gizmo drag to the segment's station. Driven by the proxy, so it also runs on undo/redo. */
	void ApplySegmentTransform(const FTransform& NewTransform);

	/** Full geometry rebuild after a drag, rather than once per frame. */
	void RebuildAfterDrag();

	/** Splits the current segment at Dist, or adds a segment to the lane under the cursor. */
	void AddSegmentAt(ARoadActor* Road, double Dist);

	/** Copies the current lane across one of its own boundaries. The structural edit of this tool. */
	void CopyLaneAcrossBoundary(ARoadActor* Road, bool bLeftBoundary);

	void DeleteSelectedSegment();

	/** Steps to the neighbouring lane: -1 is the one on the left, +1 the one on the right. */
	void MoveToNeighbourLane(int32 Step);

	void SelectNextSegment();

	/** The Segments array property of a lane, resolved per edit rather than cached. */
	static FArrayProperty* GetLaneSegmentsProperty();

	/** Lane whose segment is selected, held weakly because a rebuild can take the lane with it. */
	TWeakObjectPtr<URoadLane> CurrentLane;

	/** Index into CurrentLane's segments, or INDEX_NONE. */
	int32 SegmentIndex = INDEX_NONE;

	/** The handle the in-flight drag grabbed, or None. */
	ERoadGizmoHandle DraggedHandle = ERoadGizmoHandle::None;

	/**
	 * The segments array as it looked before the in-flight drag.
	 *
	 * A drag is one undo step, not one per frame: the snapshot is taken once on the press and emitted once
	 * on the release, which is also what makes undo restore the station to where the drag picked it up.
	 */
	TUniquePtr<FRoadArrayChange> DragChange;
};
