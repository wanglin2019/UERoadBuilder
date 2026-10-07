// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"

#include "RoadTool_GroundEdit.generated.h"

class FArrayProperty;
class FBoolProperty;
class FProperty;
class AGroundActor;
class URoadPointGizmo;

/**
 * Settings of URoadTool_GroundEdit: a live mirror of the selected ground point.
 *
 * The legacy tool had two panels for the same selection - FGroundPoint for a point that follows a road,
 * a bare FVector for a free one - and swapped between them. Both are mirrored here instead, because only
 * the free point is editable in either case: FGroundPoint's fields are plain UPROPERTY() and the legacy
 * details view showed them read-only. A single set also means the panel does not change layout as the
 * user clicks from one kind of point to the other.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_GroundEditProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Point these values mirror; INDEX_NONE when none is selected. Owned by the tool, not the user. */
	UPROPERTY(VisibleAnywhere, Category = "Ground Point")
	int32 PointIndex = INDEX_NONE;

	/** True for a free point, which is the only kind this tool lets the user move or type coordinates for. */
	UPROPERTY(VisibleAnywhere, Category = "Ground Point")
	bool bManualPoint = false;

	/** Road the point follows, when it is not a free point. */
	UPROPERTY(VisibleAnywhere, Category = "Ground Point")
	TObjectPtr<ARoadActor> RoadPointRoad = nullptr;

	/** Which side of that road the point sits on, when it is not a free point. */
	UPROPERTY(VisibleAnywhere, Category = "Ground Point")
	int32 RoadPointSide = 0;

	/** Index the point carries on that road, when it is not a free point. */
	UPROPERTY(VisibleAnywhere, Category = "Ground Point")
	int32 RoadPointIndex = 0;

	/** World position; only meaningful for a free point. */
	UPROPERTY(EditAnywhere, Category = "Ground Point")
	FVector ManualPoint = FVector::ZeroVector;
};

/** Builder for URoadTool_GroundEdit. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_GroundEditBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Edits the outline a ground area is built from.
 *
 * Legacy gesture, preserved: left click picks a ground, then one of its points; a right click on an
 * endpoint joins another ground there, closes the loop by clicking the ground's own other endpoint, or
 * inserts a free point where the cursor meets the ground's plane.
 *
 * Only free points get a transform gizmo, which is faithful: a point that follows a road is positioned by
 * the road, so there is nothing for the user to drag. Road-following points are still selectable, because
 * a join can only start from an endpoint.
 *
 * | legacy (FModeTool)                          | here                                            |
 * |---------------------------------------------|-------------------------------------------------|
 * | HitProxyCast<HGroundProxy> (ground, point)   | RoadPicking, one collector over both            |
 * | GetWidgetLocation / InputDelta               | URoadPointGizmo, world axes                     |
 * | ShowStruct(FGroundPoint / FVector)           | URoadTool_GroundEditProperties                  |
 * | FScopedTransaction for Join / AddManualPoint | FRoadUndoTransaction                            |
 * | Render(PDI) + hit proxies                    | Render(RenderAPI), no hit proxies               |
 *
 * One deliberate difference: the legacy outline was drawn, and therefore hit-tested, at a constant screen
 * thickness, so a ground stayed clickable from any distance. The ray test scales its tolerance by the
 * element's distance from the camera to keep that behaviour, but it is a tolerance on the vertex
 * polyline, not on a fixed-size glyph.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_GroundEdit : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual bool OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the point selection, then let the base step the selection up a level. */
	virtual void SelectParent() override;

	/**
	 * Whether the press landed on a gizmo handle.
	 *
	 * The gizmo's own hit test is not driven by this host, so the handle test runs here, against the same
	 * geometry Render() draws; a report of no hit leaves the press an ordinary click. Only a free point
	 * has a gizmo, so a road-following point is never grabbable - which is faithful: it is positioned by
	 * the road, and there is nothing for the user to drag.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_GroundEditProperties> Properties;

	UPROPERTY()
	TObjectPtr<URoadPointGizmo> Gizmo;

private:
	/** The ground being edited, or null. Read from the host each time, so the editor's actor selection and
	 * this tool cannot disagree about which ground is being edited. */
	AGroundActor* GetCurrentGround() const;

	/** Index of the selected point, or INDEX_NONE. */
	int32 GetPointIndex() const;

	/** Index of the selected point within the ground's free-point list, or INDEX_NONE for a road point. */
	int32 GetManualPointIndex() const;

	/**
	 * Makes Ground/Index the selection and refreshes the panel, the gizmo and the view.
	 *
	 * Index may be INDEX_NONE, which selects the outline itself with no particular point - what clicking a
	 * ground's line rather than one of its points means. Only a null Ground clears the selection.
	 */
	void SelectPoint(AGroundActor* Ground, int32 Index);

	/** Copies the selected point into Properties, so the panel shows live values. */
	void SyncProperties();

	/** Writes the edited free point back into the ground, with one undo entry. */
	void ApplyProperties(FProperty* Property);

	/** Where the gizmo sits: the free point, in world axes - the legacy tool never re-oriented its widget,
	 * which is also why a drag here is not decomposed into a road frame. */
	FTransform GetPointTransform() const;

	/** Applies a gizmo drag to the free point. Driven by the proxy, so it also runs on undo/redo. */
	void ApplyPointTransform(const FTransform& NewTransform);

	/** Full geometry rebuild after a drag, rather than once per frame. */
	void RebuildAfterDrag();

	/** Closest ground or ground point under the ray, or null; OutIndex is INDEX_NONE for the whole outline. */
	void PickUnderRay(const FRay& Ray, AGroundActor*& OutGround, int32& OutIndex) const;

	/** Closes the loop by clicking the outline's own other endpoint. */
	void CloseCurrentGround(AGroundActor* Ground);

	/** Merges Other into Ground at their touching endpoints. Other is destroyed. */
	void JoinGroundAt(AGroundActor* Ground, AGroundActor* Other);

	/** Inserts a free point into the outline, on the plane the selected point already sits in. */
	void InsertManualPointAt(AGroundActor* Ground, const FRay& Ray);

	/**
	 * Where a point sits in the world, following a road when it belongs to one.
	 *
	 * Takes a mutable ground on purpose: FGroundPoint::GetPos() resolves a station through the junction
	 * slots, and that resolution is not const in the model.
	 */
	FVector GetPointPosition(AGroundActor* Ground, int32 Index) const;

	/** The Points array property of a ground; free points live in ManualPoints instead. */
	static FArrayProperty* GetManualPointsProperty();

	/** The bClosedLoop property of a ground. */
	static FBoolProperty* GetClosedLoopProperty();

	/** Index into the selected ground's points, or INDEX_NONE. */
	int32 PointIndex = INDEX_NONE;

	/** The handle the in-flight drag grabbed, or None. */
	ERoadGizmoHandle DraggedHandle = ERoadGizmoHandle::None;

	/**
	 * The free-point array as it looked before the in-flight drag.
	 *
	 * A drag is one undo step, not one per frame: the snapshot is taken once on the press and emitted once
	 * on the release, which is also what makes undo restore the point to where the drag picked it up.
	 */
	TUniquePtr<FRoadArrayChange> DragChange;
};
