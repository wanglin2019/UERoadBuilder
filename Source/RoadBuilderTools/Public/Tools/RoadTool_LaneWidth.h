// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"

#include "RoadTool_LaneWidth.generated.h"

class URoadBoundary;
class URoadKeyInputBehavior;
class URoadPointGizmo;

/**
 * Settings of URoadTool_LaneWidth: a live mirror of one width control point on a boundary.
 *
 * The legacy tool pushed the raw FCurveOffset into a struct details view and reacted through a NotifyHook
 * callback; here the mirrored values are the UI and the tool reacts in OnPropertyModified().
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_LaneWidthProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Control point these values mirror; INDEX_NONE when none is selected. Owned by the tool. */
	UPROPERTY(VisibleAnywhere, Category = "Lane Width")
	int32 OffsetIndex = INDEX_NONE;

	/** Station along the road the control point sits at. */
	UPROPERTY(EditAnywhere, Category = "Lane Width", meta = (ClampMin = "0.0", UIMin = "0.0"))
	double Dist = 0.0;

	/** How far the boundary is from the road centreline here. */
	UPROPERTY(EditAnywhere, Category = "Lane Width")
	double Offset = 0.0;

	/** Slope of the width change, as the boundary's own direction offset. */
	UPROPERTY(EditAnywhere, Category = "Lane Width")
	double Dir = 0.0;
};

/** Builder for URoadTool_LaneWidth. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_LaneWidthBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Widens or narrows a road by editing the width control points along one of its boundaries.
 *
 * Legacy gesture, preserved: left click on a road selects it, left click again picks a boundary or one of
 * its control points, and a right click on a boundary adds a new control point there. The control points
 * only appear once their boundary is picked, which is why the ray only offers them from then on.
 *
 * | legacy (FModeTool)                                    | here                                        |
 * |-------------------------------------------------------|---------------------------------------------|
 * | HitProxyCast<HRoadCurveProxy> (whole curve, or a point)| RoadPicking, one collector over both       |
 * | GetWidgetLocation / GetCustomDrawingCoordinateSystem / InputDelta | URoadPointGizmo on the road plane |
 * | ShowStruct(FCurveOffset) + NotifyHook                  | URoadTool_LaneWidthProperties               |
 * | InputKey(Delete)                                       | URoadKeyInputBehavior                       |
 * | FScopedTransaction for AddLocalOffset                  | FRoadArrayChange + EmitObjectChange         |
 * | Render(PDI) + hit proxies                              | Render(RenderAPI), no hit proxies           |
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_LaneWidth : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual bool OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the control point selection, then let the base step the road selection up a level. */
	virtual void SelectParent() override;

	/**
	 * Whether the press landed on a gizmo handle.
	 *
	 * The gizmo's own hit test is not driven by this host, so the handle test runs here, against the same
	 * geometry Render() draws; a report of no hit leaves the press an ordinary click. A drag decomposes
	 * into the boundary frame through ApplyOffsetTransform: X slides the control point along the road,
	 * Y widens or narrows the boundary.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_LaneWidthProperties> Properties;

	UPROPERTY()
	TObjectPtr<URoadPointGizmo> Gizmo;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> DeleteKeyBehavior;

private:
	/** The boundary being edited, or null when the selection no longer resolves. */
	URoadBoundary* GetCurrentBoundary() const;

	/** Index of the selected control point, or INDEX_NONE when it no longer resolves. */
	int32 GetOffsetIndex() const;

	/** Makes Boundary/Index the selection and refreshes the panel, the gizmo and the view. */
	void SelectOffset(URoadBoundary* Boundary, int32 Index);

	/** Copies the selected control point into Properties, so the panel shows live values. */
	void SyncPropertiesFromOffset();

	/** Writes the panel back into the road, with one undo entry. */
	void ApplyPropertiesToOffset();

	/** Where the gizmo sits: the control point, rotated onto the boundary's own tangent. */
	FTransform GetOffsetTransform() const;

	/** Applies a gizmo drag to the control point. Driven by the proxy, so it also runs on undo/redo. */
	void ApplyOffsetTransform(const FTransform& NewTransform);

	/** Full geometry rebuild after a drag, rather than once per frame. */
	void RebuildAfterDrag();

	/**
	 * Resolves the ray against every boundary, and against the current boundary's control points.
	 *
	 * Both kinds of element go through one collector so that whichever is nearer wins: a control point
	 * sitting on top of its boundary is picked as the point, and the bare boundary as the boundary. That is
	 * what the two hit-proxy flavours (HRoadCurveProxy with and without an index) expressed.
	 */
	void PickUnderRay(const FRay& Ray, URoadBoundary*& OutBoundary, int32& OutIndex) const;

	void AddControlPoint(URoadBoundary* Boundary, double Dist);

	void DeleteSelectedOffset();

	/** The LocalOffsets array property of a boundary, resolved per edit rather than cached. */
	static FArrayProperty* GetLocalOffsetsProperty();

	/** Boundary whose control point is selected, held weakly because a rebuild can take it with it. */
	TWeakObjectPtr<URoadBoundary> CurrentBoundary;

	/** Index into CurrentBoundary's local offsets, or INDEX_NONE. */
	int32 OffsetIndex = INDEX_NONE;

	/** The handle the in-flight drag grabbed, or None. */
	ERoadGizmoHandle DraggedHandle = ERoadGizmoHandle::None;

	/**
	 * The offsets array as it looked before the in-flight drag.
	 *
	 * A drag is one undo step, not one per frame: the snapshot is taken once on the press and emitted once
	 * on the release, which is also what makes undo restore the point to where the drag picked it up.
	 */
	TUniquePtr<FRoadArrayChange> DragChange;
};
