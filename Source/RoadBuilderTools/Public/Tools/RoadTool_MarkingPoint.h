// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"
#include "Tools/RoadPropertyChange.h"

#include "RoadTool_MarkingPoint.generated.h"

class UMarkingPoint;
class URoadKeyInputBehavior;
class URoadPointGizmo;

/**
 * Settings of URoadTool_MarkingPoint: a live mirror of the selected marking point.
 *
 * The legacy tool pushed the whole UMarkingPoint into an object details view. Here the two editable fields
 * are mirrored, which is what the object view was actually for - it carried no other UPROPERTY - and the
 * tool notices edits in OnPropertyModified() instead of through a NotifyHook.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_MarkingPointProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Marking these values mirror; INDEX_NONE when none is selected. Owned by the tool, not the user. */
	UPROPERTY(VisibleAnywhere, Category = "Marking Point")
	int32 MarkingIndex = INDEX_NONE;

	/** What is placed here. */
	UPROPERTY(EditAnywhere, Category = "Marking Point")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Where it sits, as (station, lateral offset) on the road. */
	UPROPERTY(EditAnywhere, Category = "Marking Point")
	FVector2D Point = FVector2D::ZeroVector;
};

/** Builder for URoadTool_MarkingPoint. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_MarkingPointBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Places and edits point markings - a manhole cover, a bollard - on a road.
 *
 * Legacy gesture, preserved: left click on a road selects it, left click again picks one of its marking
 * points, and a right click drops a new one where the cursor meets the road. Delete removes the selected
 * marking.
 *
 * | legacy (FModeTool)                                     | here                                       |
 * |--------------------------------------------------------|--------------------------------------------|
 * | HitProxyCast<HRoadMarkingProxy>                         | one FRoadHitCollector over the road's markings |
 * | GetWidgetLocation / GetCustomDrawingCoordinateSystem / InputDelta | URoadPointGizmo on the road plane |
 * | ShowObject(UMarkingPoint) + NotifyHook                  | URoadTool_MarkingPointProperties            |
 * | InputKey(Delete)                                        | URoadKeyInputBehavior                       |
 * | FScopedTransaction for AddMarkingPoint                  | FRoadUndoTransaction                        |
 * | Render(PDI) + hit proxies                               | Render(RenderAPI), no hit proxies           |
 *
 * The two fields are mirrored rather than registered as the marking object itself: ITF can add any object
 * as a property source but can only remove a UInteractiveToolPropertySet, so a panel that has to swap with
 * the selection has to be a property set. Mirroring also keeps the undo record explicit, which an
 * engine-drawn object view would not.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_MarkingPoint : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual bool OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the marking selection, then let the base step the road selection up a level. */
	virtual void SelectParent() override;

	/**
	 * Whether the press landed on a gizmo handle.
	 *
	 * The gizmo's own hit test is not driven by this host, so the handle test runs here, against the same
	 * geometry Render() draws; a report of no hit leaves the press an ordinary click. A drag decomposes
	 * into the road frame through ApplyMarkingTransform: X slides the marking along the road, Y moves it
	 * across.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_MarkingPointProperties> Properties;

	UPROPERTY()
	TObjectPtr<URoadPointGizmo> Gizmo;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> DeleteKeyBehavior;

private:
	/** The marking being edited, or null when the selection no longer resolves. */
	UMarkingPoint* GetCurrentMarking() const;

	/** The road the current marking belongs to, or null. */
	ARoadActor* GetCurrentMarkingRoad() const;

	/** Makes Marking the selection and refreshes the panel, the gizmo and the view. */
	void SelectMarking(UMarkingPoint* Marking);

	/** Copies the selected marking into Properties, so the panel shows live values. */
	void SyncPropertiesFromMarking();

	/** Writes the panel back into the marking, with one undo entry. */
	void ApplyPropertiesToMarking(FProperty* Property);

	/** Where the gizmo sits: the marking, rotated onto the road tangent. */
	FTransform GetMarkingTransform() const;

	/** Applies a gizmo drag to the marking. Driven by the proxy, so it also runs on undo/redo. */
	void ApplyMarkingTransform(const FTransform& NewTransform);

	/** Full geometry rebuild after a drag, rather than once per frame. */
	void RebuildAfterDrag();

	/** Closest marking point of the selected road under the ray, or null. */
	UMarkingPoint* PickMarkingUnderRay(const FRay& Ray) const;

	void AddMarkingPointAt(ARoadActor* Road, const FVector2D& UV);

	void DeleteSelectedMarking();

	/** Marking being edited, held weakly because a rebuild can take it with it. */
	TWeakObjectPtr<UMarkingPoint> CurrentMarking;

	/** The handle the in-flight drag grabbed, or None. */
	ERoadGizmoHandle DraggedHandle = ERoadGizmoHandle::None;

	/**
	 * The marking's Point property as it looked before the in-flight drag.
	 *
	 * A drag is one undo step, not one per frame: the snapshot is taken once on the press and emitted once
	 * on the release. A property record rather than an array one, because a marking point edits one field
	 * of one object - the same shape the panel's own edits are recorded with.
	 */
	TUniquePtr<FRoadPropertyChange> DragChange;
};
