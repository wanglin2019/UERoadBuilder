// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"

#include "RoadTool_MarkingLane.generated.h"

class URoadBoundary;
class URoadKeyInputBehavior;
class URoadPointGizmo;

/**
 * Settings of URoadTool_MarkingLane: a live mirror of one marking segment along a boundary.
 *
 * The legacy tool pushed the raw FBoundarySegment into a struct details view and reacted through a
 * NotifyHook callback; here the mirrored values are the UI and the tool reacts in OnPropertyModified().
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_MarkingLaneProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Segment these values mirror; INDEX_NONE when none is selected. Owned by the tool, not the user. */
	UPROPERTY(VisibleAnywhere, Category = "Marking Lane")
	int32 SegmentIndex = INDEX_NONE;

	/** Station along the road where the segment starts. */
	UPROPERTY(EditAnywhere, Category = "Marking Lane", meta = (ClampMin = "0.0", UIMin = "0.0"))
	double Dist = 0.0;

	/** What is painted along the boundary over this segment. */
	UPROPERTY(EditAnywhere, Category = "Marking Lane")
	TObjectPtr<ULaneMarkStyle> LaneMarking = nullptr;

	/** Props placed along the boundary over this segment. */
	UPROPERTY(EditAnywhere, Category = "Marking Lane")
	TObjectPtr<URoadProps> Props = nullptr;
};

/** Builder for URoadTool_MarkingLane. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_MarkingLaneBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Edits the lane markings a boundary carries, segment by segment.
 *
 * Legacy gesture, preserved: left click on a road selects it, left click again picks a boundary segment,
 * and a right click on a boundary splits it with a new segment at the cursor. Home/End step sideways
 * through the boundaries either side of a lane, Tab walks the segments, Delete removes one.
 *
 * | legacy (FModeTool)                                    | here                                        |
 * |-------------------------------------------------------|---------------------------------------------|
 * | HitProxyCast<HRoadCurveProxy>(Boundary, Index)          | one FRoadHitCollector over segments         |
 * | GetWidgetLocation / GetCustomDrawingCoordinateSystem / InputDelta | URoadPointGizmo, one axis along the road |
 * | ShowStruct(FBoundarySegment) + NotifyHook               | URoadTool_MarkingLaneProperties             |
 * | InputKey(Home/End/Tab/Delete)                           | four URoadKeyInputBehavior instances        |
 * | FScopedTransaction for AddSegment                       | FRoadArrayChange + EmitObjectChange         |
 * | Render(PDI) + hit proxies                               | Render(RenderAPI), no hit proxies           |
 *
 * The gap to LaneEdit is entirely in what a segment means: there a lane cross-section, here a stretch of
 * paint. The pick, the gizmo, the panel and the key walk are the same machine, which is why both tools
 * give the shared layer the same calls with different data.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_MarkingLane : public URoadInteractiveTool
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
	 * the boundary tangent, so a drag moves the segment's start along the road.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_MarkingLaneProperties> Properties;

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
	/** The boundary being edited, or null when the selection no longer resolves. */
	URoadBoundary* GetCurrentBoundary() const;

	/** Makes Boundary/Index the selection and refreshes the panel, the gizmo and the view. */
	void SelectSegment(URoadBoundary* Boundary, int32 InSegmentIndex);

	/** Copies the selected segment into Properties, so the panel shows live values. */
	void SyncPropertiesFromSegment();

	/** Writes the panel back into the road, with one undo entry. */
	void ApplyPropertiesToSegment();

	/** Where the gizmo sits: the segment start, rotated onto the boundary's own tangent. */
	FTransform GetSegmentTransform() const;

	/** Applies a gizmo drag to the segment's station. Driven by the proxy, so it also runs on undo/redo. */
	void ApplySegmentTransform(const FTransform& NewTransform);

	/** Full geometry rebuild after a drag, rather than once per frame. */
	void RebuildAfterDrag();

	/** Resolves the ray against every segment of every boundary. */
	void PickUnderRay(const FRay& Ray, URoadBoundary*& OutBoundary, int32& OutIndex) const;

	void AddSegmentAt(URoadBoundary* Boundary, double Dist);

	void DeleteSelectedSegment();

	/** Steps to the boundary past the neighbouring lane: -1 goes left, +1 goes right. */
	void MoveToNeighbourBoundary(int32 Step);

	void SelectNextSegment();

	/** The Segments array property of a boundary, resolved per edit rather than cached. */
	static FArrayProperty* GetBoundarySegmentsProperty();

	/** Boundary whose segment is selected, held weakly because a rebuild can take it with it. */
	TWeakObjectPtr<URoadBoundary> CurrentBoundary;

	/** Index into CurrentBoundary's segments, or INDEX_NONE. */
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
