// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"

#include "RoadTool_RoadPlan.generated.h"

class FArrayProperty;
class URoadKeyInputBehavior;
class URoadPointGizmo;

/**
 * Settings of URoadTool_RoadPlan: a live mirror of the selected alignment point.
 *
 * The legacy tool pushed FRoadPoint straight into a struct details view. Mirroring it costs one small
 * class and buys an explicit undo record: an edit typed into the panel and an edit made by dragging then
 * travel the same path, so both land on the undo stack the same way.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_RoadPlanProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Point these values mirror; INDEX_NONE when none is selected. Owned by the tool, not the user. */
	UPROPERTY(VisibleAnywhere, Category = "Road Point")
	int32 PointIndex = INDEX_NONE;

	/** Station the point resolved to along the road, after the curve was fitted. Written by the model. */
	UPROPERTY(VisibleAnywhere, Category = "Road Point")
	double Dist = 0.0;

	/** Plan position of the point, in world X/Y. */
	UPROPERTY(EditAnywhere, Category = "Road Point")
	FVector2D Pos = FVector2D::ZeroVector;

	/** How far the fitted curve may bulge away from the straight line between neighbours. */
	UPROPERTY(EditAnywhere, Category = "Road Point")
	double MaxRadius = 50000.0;

	/** How much of the curvature is blended across this point. */
	UPROPERTY(EditAnywhere, Category = "Road Point")
	double CurvatureBlend = 0.0;
};

/** Builder for URoadTool_RoadPlan. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadPlanBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Lays out the road network: places, moves and removes road alignment points.
 *
 * Legacy gesture, preserved. A left click picks a point of the selected road, or a road to work on, or
 * clears the selection when it hits neither. A right click on a road splits its alignment there, and a
 * right click on empty space either inserts a point into the selected road or, when there is no road yet,
 * creates one. Delete removes the selected point, or the whole road when no point is selected.
 *
 * Endpoint movement also re-runs the connection search: dragging an end of a road onto another road
 * connects them, which is what makes the tool the entry point of the whole network.
 *
 * | legacy (FModeTool)                              | here                                      |
 * |-------------------------------------------------|-------------------------------------------|
 * | HandleClickRoad(HRoadProxy, with or without index) | RoadPicking, one collector over both    |
 * | GetWidgetLocation / InputDelta                  | URoadPointGizmo, world axes               |
 * | ShowStruct(FRoadPoint)                          | URoadTool_RoadPlanProperties              |
 * | mode's SetCurrentToolByType -> USettings_RoadPlan | registered by this tool directly        |
 * | InputKey(Delete)                                | URoadKeyInputBehavior                     |
 * | FScopedTransaction for AddRoad / destroy        | FRoadUndoTransaction                      |
 * | Render(PDI) + hit proxies                       | Render(RenderAPI), no hit proxies         |
 *
 * This replaces the S0 pilot implementation, which kept its own list of points to prove out the host,
 * input, render and context-injection paths before real road data was migrated. Those paths are all still
 * exercised - by the same calls - so the pilot's job is done and the class now edits the model.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadPlan : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual bool OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the point selection, then let the base step the road selection up a level. */
	virtual void SelectParent() override;

	/**
	 * Whether the press landed on a gizmo handle.
	 *
	 * This is the whole reason the tool draws and drags its own handles: the gizmo's own hit test is not
	 * driven by this host, so the handle test runs here, against the same geometry Render() draws. A
	 * report of no hit means the press becomes an ordinary click and selection carries on unchanged.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_RoadPlanProperties> Properties;

	UPROPERTY()
	TObjectPtr<URoadPointGizmo> Gizmo;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> DeleteKeyBehavior;

private:
	/** Index of the selected point within the selected road, or INDEX_NONE. */
	int32 GetPointIndex() const;

	/** Makes Road/Index the selection and refreshes the panel, the gizmo and the view. */
	void SelectRoadAndPoint(ARoadActor* Road, int32 Index);

	/** Drops both halves of the selection, which is what a click on empty space means here. */
	void ResetSelection();

	/** Copies the selected point into Properties, so the panel shows live values. */
	void SyncProperties();

	/** Writes the edited point back into the road, with one undo entry. */
	void ApplyProperties(FProperty* Property);

	/** Where the gizmo sits: the point, in world axes - the legacy tool never re-oriented its widget. */
	FTransform GetPointTransform() const;

	/** Applies a gizmo drag to the point. Driven by the proxy, so it also runs on undo/redo. */
	void ApplyPointTransform(const FTransform& NewTransform);

	/**
	 * Re-runs the connection search for a point that was just moved.
	 *
	 * Only endpoints are candidates - a point in the middle of a road has nothing to connect - and the
	 * search uses the model's own proximity query, because a connection is a road-to-road relationship
	 * rather than something the view can answer.
	 */
	void UpdateEndpointConnection(ARoadActor* Road, int32 Index);

	/** Full geometry rebuild after a drag, rather than once per frame. */
	void RebuildAfterDrag();

	/** Road and point under the ray; OutRoad is null when the ray missed everything. */
	void PickUnderRay(const FRay& Ray, ARoadActor*& OutRoad, int32& OutPointIndex) const;

	/** Where the ray meets the plan plane the alignment is laid out on. */
	FVector GetPlanPosition(const FRay& Ray) const;

	/** Splits Road's alignment at the plan position. Does nothing to a road too short to have a segment. */
	void SplitRoadAt(ARoadActor* Road, const FVector& Position);

	/** Creates a road styled by the plan settings and gives it its first point. */
	void CreateRoadAt(const FVector& Position);

	/** Inserts a point into the selected road, next to the selected one. */
	void InsertPointAt(ARoadActor* Road, const FVector& Position);

	void DeleteSelection();

	/** The RoadPoints array property of a road, resolved per edit rather than cached. */
	static FArrayProperty* GetRoadPointsProperty();

	/** Index into the selected road's alignment points, or INDEX_NONE. */
	int32 PointIndex = INDEX_NONE;

	/** The handle the in-flight drag grabbed, or None. */
	ERoadGizmoHandle DraggedHandle = ERoadGizmoHandle::None;

	/**
	 * The alignment array as it looked before the in-flight drag.
	 *
	 * A drag is one undo step, not one per frame. The tool's value edits normally record themselves per
	 * call, but a drag calls the write path dozens of times, so the snapshot is taken once on the press
	 * and emitted once on the release - which is also what makes undo restore the point to where the drag
	 * picked it up rather than to some intermediate frame.
	 */
	TUniquePtr<FRoadArrayChange> DragChange;
};
