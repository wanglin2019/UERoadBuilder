// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "InteractiveTool.h"
#include "RoadActor.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"

#include "RoadTool_RoadHeight.generated.h"

class ARoadScene;
class FArrayProperty;
class URoadKeyInputBehavior;
class URoadPointGizmo;

/**
 * Settings of URoadTool_RoadHeight: a live mirror of the selected height point.
 *
 * The legacy tool pushed the raw FHeightPoint into a struct details view and reacted to changes through
 * a NotifyHook callback. Here the mirrored values *are* the UI: the host renders whatever property set
 * the tool registers, and the tool notices edits in OnPropertyModified(). Nothing in this class knows
 * whether an editor details panel or a runtime widget is showing it, which is what lets the panel be a
 * host concern.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_RoadHeightProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Distance along the road, in cm. Clamped to the road's length when edited. */
	UPROPERTY(EditAnywhere, Category = "Road Height", meta = (ClampMin = "0.0", UIMin = "0.0"))
	double Dist = 0.0;

	/** Elevation at this point. */
	UPROPERTY(EditAnywhere, Category = "Road Height")
	double Height = 0.0;

	/** Influence range along the road. */
	UPROPERTY(EditAnywhere, Category = "Road Height", meta = (ClampMin = "1.0", UIMin = "1.0"))
	double Range = 3000.0;

	/** Height point these values mirror; INDEX_NONE when none is selected. Owned by the tool, not the user. */
	UPROPERTY(VisibleAnywhere, Category = "Road Height")
	int32 PointIndex = INDEX_NONE;
};

/** Builder for URoadTool_RoadHeight. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadHeightBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Edits the height profile of the selected road.
 *
 * This is the S0.5 pilot: the hardest legacy tool to port, chosen because it is the only one that
 * exercises the entire risk surface of the migration at once.
 *
 * | legacy (FModeTool)                          | here                                          |
 * |---------------------------------------------|-----------------------------------------------|
 * | HandleClick + HHitProxy picking              | base click routing + RoadPicking ray tests (ITF has no hit proxies) |
 * | GetWidgetLocation / GetCustomDrawingCoordinateSystem | a UCombinedTransformGizmo rotated onto the road tangent |
 * | InputDelta                                   | the proxy's SetTransformFunc                  |
 * | NotifyPreChange + FScopedTransaction         | FRoadArrayChange + EmitObjectChange           |
 * | ShowStruct(FHeightPoint) details view        | URoadTool_RoadHeightProperties                |
 * | InputKey(Delete)                             | URoadKeyInputBehavior                         |
 * | Render(PDI) with hit proxies                 | Render(RenderAPI) without them                |
 *
 * What is deliberately *not* here: any editor type, any host pointer. Everything the tool needs beyond
 * road data arrives through IRoadEditorContext and ITF.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadHeight : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	/** UInteractiveTool implementation */
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	/**
	 * URoadInteractiveTool implementation.
	 * Left click picks a height point (or falls back to selecting a road); right click inserts one.
	 */
	virtual bool OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the height point selection, then let the base step the road selection up a level. */
	virtual void SelectParent() override;

	/**
	 * Whether the press landed on the gizmo's axis handle.
	 *
	 * The gizmo's own hit test is not driven by this host, so the handle test runs here, against the same
	 * geometry Render() draws; a report of no hit leaves the press an ordinary click. The drag reuses the
	 * existing write path - ApplyPointTransform projects the gizmo's target back onto the road and clamps
	 * it, so the point slides along the curve and can never leave it.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_RoadHeightProperties> Properties;

	/**
	 * The drag gizmo, rotated onto the road tangent so its single axis means "along the road".
	 *
	 * This is where the legacy GetWidgetLocation / GetCustomDrawingCoordinateSystem / InputDelta trio
	 * ended up: the shared helper owns the proxy and the gizmo, and the two callbacks below are the two
	 * halves that are actually specific to a height point.
	 */
	UPROPERTY()
	TObjectPtr<URoadPointGizmo> Gizmo;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> DeleteKeyBehavior;

private:
	/** Makes PointIndex the selection, or clears it when the index is not valid for the road. */
	void SelectPoint(ARoadActor* Road, int32 PointIndex);

	/** Copies the selected height point into Properties, so the panel shows live values. */
	void SyncPropertiesFromPoint();

	/** Index of the selected height point, or INDEX_NONE when the selection no longer resolves. */
	int32 GetSelectedPointIndex() const;

	/** Transform the gizmo sits on: the point's position, rotated onto the road tangent. */
	FTransform GetPointTransform() const;

	/** Applies a gizmo edit to the selected point: the drag's per-frame write, projection-based. */
	void ApplyPointTransform(const FTransform& NewTransform);

	/** Gizmo drag finished: the deferred geometry rebuild happens here. */
	void RebuildAfterDrag();

	/** Writes the mirrored properties back into the road, with a single undo entry. */
	void ApplyPropertiesToPoint();

	/**
	 * Inserts a height point at Dist and selects it, with a single undo entry.
	 *
	 * Note on the insert index: ARoadActor::AddHeight() returns the index of the point *before* the new
	 * one, so the new point is at +1. The legacy tool took the return value as-is and therefore selected
	 * the wrong point after inserting; the +1 here is the correction.
	 */
	void AddHeightPoint(ARoadActor* Road, double Dist);

	void DeleteSelectedPoint();

	/** The HeightPoints array property, resolved once per edit rather than cached (edits are user-paced). */
	static FArrayProperty* GetHeightPointsProperty();

	/**
	 * Closest height point of Road under the ray; INDEX_NONE when none is close enough.
	 *
	 * Stays here rather than moving into RoadPicking: height points are this tool's own element type, not
	 * one of the shared road / boundary / ground / junction / marking sets the others pick.
	 */
	int32 PickHeightPoint(const ARoadActor* Road, const FRay& Ray) const;

	/** The handle the in-flight drag grabbed, or None. */
	ERoadGizmoHandle DraggedHandle = ERoadGizmoHandle::None;

	/**
	 * The HeightPoints array as it looked before the in-flight drag.
	 *
	 * A drag is one undo step, not one per frame: the snapshot is taken once on the press and emitted once
	 * on the release, which is also what makes undo restore the point to where the drag picked it up.
	 */
	TUniquePtr<FRoadArrayChange> DragChange;
};
