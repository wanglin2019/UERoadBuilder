// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoadActor.h"
#include "Tools/RoadArrayChange.h"
#include "Tools/RoadInteractiveTool.h"
#include "Tools/RoadPointGizmo.h"

#include "RoadTool_MarkingCurve.generated.h"

class FArrayProperty;
class FBoolProperty;
class UMarkingCurve;
class URoadKeyInputBehavior;
class URoadPointGizmo;

/**
 * Settings of URoadTool_MarkingCurve: the selected curve as a whole.
 *
 * The legacy tool pushed the whole UMarkingCurve into an object details view; here its editable fields
 * are mirrored, which is also what makes the undo record explicit - an engine-drawn object view would
 * edit the object behind the tool's back.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_MarkingCurveProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Marking these values mirror; INDEX_NONE when none is selected. Owned by the tool, not the user. */
	UPROPERTY(VisibleAnywhere, Category = "Marking Curve")
	int32 MarkingIndex = INDEX_NONE;

	/** Line style of the curve. */
	UPROPERTY(EditAnywhere, Category = "Marking Curve")
	TObjectPtr<UBaseMarkStyle> MarkStyle = nullptr;

	/** Fill style, used when the curve is closed. */
	UPROPERTY(EditAnywhere, Category = "Marking Curve")
	TObjectPtr<UPolygonMarkStyle> FillStyle = nullptr;

	/** Rotation of the fill, used when the curve is closed. */
	UPROPERTY(EditAnywhere, Category = "Marking Curve")
	double Orientation = 0.0;

	/** Whether the last point joins back to the first. */
	UPROPERTY(EditAnywhere, Category = "Marking Curve")
	bool bClosedLoop = false;
};

/**
 * Settings of URoadTool_MarkingCurve: one control point of the curve.
 *
 * A second property set rather than a section of the first, because ITF can only take a
 * UInteractiveToolPropertySet back off the panel, so a panel that has to follow the selection has to be
 * a property set that can be swapped. Two selection kinds means two sets.
 */
UCLASS(Transient)
class ROADBUILDERTOOLS_API URoadTool_MarkingCurvePointProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Control point these values mirror; INDEX_NONE when none is selected. Owned by the tool. */
	UPROPERTY(VisibleAnywhere, Category = "Control Point")
	int32 PointIndex = INDEX_NONE;

	/** Which handle of the point is selected: 0 the point, 1 its incoming tangent, 2 its outgoing one. */
	UPROPERTY(VisibleAnywhere, Category = "Control Point")
	int32 HandleIndex = 0;

	/** Position on the road, as (station, lateral offset). */
	UPROPERTY(EditAnywhere, Category = "Control Point")
	FVector2D Pos = FVector2D::ZeroVector;

	/** Incoming tangent handle, relative to Pos. */
	UPROPERTY(EditAnywhere, Category = "Control Point")
	FVector2D In = FVector2D::ZeroVector;

	/** Outgoing tangent handle, relative to Pos. */
	UPROPERTY(EditAnywhere, Category = "Control Point")
	FVector2D Out = FVector2D::ZeroVector;
};

/** Builder for URoadTool_MarkingCurve. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_MarkingCurveBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Draws and edits spline markings - lane lines, gore areas, hatched boxes - on a road.
 *
 * Legacy gesture, preserved: left click picks a road, then a curve, then one of its control points; a
 * right click extends the curve from an endpoint, closes it by clicking the other endpoint, or starts a
 * new curve when nothing is selected. Delete removes the whole curve.
 *
 * A control point owns three handles - the point itself and its two tangent handles - and the legacy tool
 * only ever showed all three for the point that was selected, which is why the ray is only offered the
 * tangents of that one point.
 *
 * | legacy (FModeTool)                                     | here                                        |
 * |--------------------------------------------------------|---------------------------------------------|
 * | HitProxyCast<HRoadMarkingProxy> (curve, point, handle)  | RoadPicking, one collector over both        |
 * | GetWidgetLocation / GetCustomDrawingCoordinateSystem / InputDelta | URoadPointGizmo on the road plane |
 * | ShowObject(UMarkingCurve) / ShowStruct(FMarkingCurvePoint) | two property sets, swapped on selection  |
 * | InputKey(Delete)                                       | URoadKeyInputBehavior                       |
 * | FScopedTransaction for AddMarkingCurve                 | FRoadUndoTransaction                        |
 * | Render(PDI) + hit proxies                              | Render(RenderAPI), no hit proxies           |
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_MarkingCurve : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;
	virtual void OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

	/** Escape: drop the marking selection, then let the base step the road selection up a level. */
	virtual void SelectParent() override;

	/**
	 * Whether the press landed on a gizmo handle.
	 *
	 * The gizmo's own hit test is not driven by this host, so the handle test runs here, against the same
	 * geometry Render() draws; a report of no hit leaves the press an ordinary click. A drag decomposes
	 * into the road frame through ApplyMarkingTransform: X slides along the road, Y moves across it, and
	 * the handle the gizmo sits on decides which field the motion lands in - or the whole curve moves as
	 * a unit when only the curve is selected.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos) override;

	/** Moves the grabbed handle with the cursor. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos) override;

	/** Commits the drag: closes the undo bracket and rebuilds the geometry. */
	virtual void OnRoadDragEnded() override;

protected:
	UPROPERTY()
	TObjectPtr<URoadTool_MarkingCurveProperties> Properties;

	UPROPERTY()
	TObjectPtr<URoadTool_MarkingCurvePointProperties> PointProperties;

	UPROPERTY()
	TObjectPtr<URoadPointGizmo> Gizmo;

	UPROPERTY()
	TObjectPtr<URoadKeyInputBehavior> DeleteKeyBehavior;

private:
	/** The curve being edited, or null when the selection no longer resolves. */
	UMarkingCurve* GetCurrentMarking() const;

	/** The road the current curve belongs to, or null. */
	ARoadActor* GetCurrentMarkingRoad() const;

	/** Index of the selected control point, or INDEX_NONE when the whole curve is selected. */
	int32 GetPointIndex() const;

	/** Which of the selected point's three handles the gizmo sits on. */
	int32 GetHandleIndex() const;

	/** Makes the given selection and refreshes the panel, the gizmo and the view. A null curve clears it. */
	void SelectMarking(UMarkingCurve* Marking, int32 PointIndex, int32 HandleIndex);

	/** Registers the property set matching the current selection, taking the other one off the panel. */
	void RefreshPanel();

	/** Copies the current selection into the property sets, so the panel shows live values. */
	void SyncProperties();

	/** Writes the edited set back into the curve, with one undo entry. */
	void ApplyProperties(UObject* PropertySet, FProperty* Property);

	/** Where the gizmo sits: the selected handle, or the curve centre when the whole curve is selected. */
	FTransform GetMarkingTransform() const;

	/** Applies a gizmo drag. Driven by the proxy, so it also runs on undo/redo. */
	void ApplyMarkingTransform(const FTransform& NewTransform);

	/** Full geometry rebuild after a drag, rather than once per frame. */
	void RebuildAfterDrag();

	/**
	 * Resolves the ray against the curves of the selected road, and against the handles of the current
	 * point.
	 *
	 * Both kinds go through one collector so the nearer one wins - the same relationship the legacy
	 * per-element hit proxies had, where a handle drawn on top of its own curve took the click.
	 */
	void PickUnderRay(const FRay& Ray, UMarkingCurve*& OutMarking, int32& OutPointIndex,
		int32& OutHandleIndex) const;

	/** Starts a new curve on Road and places its first point where the ray meets the world. */
	void AddMarkingCurveAt(ARoadActor* Road, const FRay& Ray);

	/** Inserts a point next to the selected endpoint, where the ray meets the world. */
	void InsertPointAt(ARoadActor* Road, const FRay& Ray);

	/** Closes the current curve by clicking its other endpoint. */
	void CloseCurrentMarking();

	void DeleteSelectedMarking();

	/** The Points array property of a curve, resolved per edit rather than cached. */
	static FArrayProperty* GetPointsProperty();

	/** The bClosedLoop property of a curve. */
	static FBoolProperty* GetClosedLoopProperty();

	/** Curve being edited, held weakly because a rebuild can take it with it. */
	TWeakObjectPtr<UMarkingCurve> CurrentMarking;

	/** Index into CurrentMarking's points, or INDEX_NONE for the whole curve. */
	int32 PointIndex = INDEX_NONE;

	/** Which handle of the selected point is being dragged: 0 the point, 1 In, 2 Out. */
	int32 HandleIndex = 0;

	/** Panel currently registered, so the swap in RefreshPanel() knows what to take off. */
	UPROPERTY()
	TObjectPtr<UInteractiveToolPropertySet> ActivePanel;

	/** The handle the in-flight drag grabbed, or None. */
	ERoadGizmoHandle DraggedHandle = ERoadGizmoHandle::None;

	/**
	 * The points array as it looked before the in-flight drag.
	 *
	 * A drag is one undo step, not one per frame: the snapshot is taken once on the press and emitted once
	 * on the release - and it covers the whole-curve drag too, which writes every point's Pos at once.
	 */
	TUniquePtr<FRoadArrayChange> DragChange;
};
