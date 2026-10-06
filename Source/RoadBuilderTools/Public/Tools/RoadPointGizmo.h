// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveGizmo.h"
#include "Math/Ray.h"
#include "Templates/Function.h"

#include "RoadPointGizmo.generated.h"

class UCombinedTransformGizmo;
class UInteractiveTool;
class USceneComponent;
class UTransformProxy;
class FPrimitiveDrawInterface;
class FSceneView;

/** Which handle of the point gizmo the cursor is over. Mirrors the elements the tool asked for. */
enum class ERoadGizmoHandle : uint8
{
	None,
	/** The X arrow: drags along world X. */
	AxisX,
	/** The Y arrow: drags along world Y. */
	AxisY,
	/** The XY square: drags in the ground plane. */
	PlaneXY,
};

/**
 * Where a click landed on the gizmo, and how far away it was.
 *
 * Distance is measured in pixels from the click ray to the handle, so the caller can rank handles the way
 * the pixel-sized hit proxies a legacy widget used to be ranked.
 */
struct FRoadGizmoHit
{
	bool bHit = false;
	ERoadGizmoHandle Handle = ERoadGizmoHandle::None;
	double PixelDistance = 0.0;
};

/**
 * The transform gizmo a point-editing road tool drags, and the deferred-rebuild bracket around a drag.
 *
 * Seven of the legacy tools did the same three things: report a widget location, restrict the widget to
 * one or two axes aligned to the road, and turn the widget's drag into a change of one stored point. That
 * is the legacy GetWidgetLocation / GetCustomDrawingCoordinateSystem / InputDelta protocol, and its ITF
 * counterpart is a UTransformProxy wired to a UCombinedTransformGizmo. The wiring is identical every
 * time - only the read and write of the point differ - so it lives here instead of in seven copies, in
 * the same spirit as the shared picking layer.
 *
 * The tool supplies the two halves (read the point, write the point). It can ignore the second argument of
 * the write callback when it wants a pure "moved by this much" reading: UTransformProxy reports an
 * absolute transform, so a tool whose data is a station takes the difference between where the gizmo
 * wants the point and where the point currently is. Comparing against the data - not against the previous
 * event - is what keeps a clamped edit from drifting the gizmo away from the point it is editing.
 *
 * Undo needs nothing from the tool: the proxy records its own change and replays it by calling back into
 * the write callback, so a transform edit undoes and redoes through the same code path that made it.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadPointGizmo : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Wires the gizmo up and leaves it hidden; a tool calls Update() once it knows whether it has a target.
	 *
	 * @param InOwningTool   the tool the proxy and gizmo belong to, and the tool the gizmo manager reports
	 *                       undo against.
	 * @param InElements     which handles to expose. TranslateAxisX for "drag along the road", which is
	 *                       what the legacy single-axis tools expressed through their custom coordinate
	 *                       system; TranslatePlaneXY for the two-axis ones.
	 * @param InGetTransform reads the point the gizmo sits on.
	 * @param InSetTransform writes the point the gizmo was dragged to. Also runs on undo and redo.
	 * @param InOnDragEnded  optional; runs when a drag finishes, which is where the expensive geometry
	 *                       rebuild belongs - never per frame.
	 */
	void Initialize(UInteractiveTool* InOwningTool, ETransformGizmoSubElements InElements,
		TFunction<FTransform()> InGetTransform,
		TFunction<void(const FTransform&)> InSetTransform,
		TFunction<void()> InOnDragEnded = nullptr);

	/** Hands the gizmo back to the gizmo manager. Safe before Initialize() and safe to call twice. */
	void Shutdown();

	/** Moves the gizmo onto GetTransform(), creating it on first use; hides it when there is no target. */
	void Update(bool bHasTarget);

	/**
	 * Draws the handles through the tool's own render pass, in immediate mode.
	 *
	 * A UCombinedTransformGizmo is visualised entirely by the ACombinedTransformGizmoActor it spawns into
	 * the world - the class has no Render() override, unlike the editor's own UTransformGizmo, which
	 * draws itself through the PDI every frame. That makes the handles dependent on the scene pass
	 * collecting the actor for the view the tool renders into, which does not happen here: the components
	 * are created, visible, registered, in the editor world and have a live SceneProxy, yet the actor is
	 * never drawn. The tool's Render() is called every frame regardless (the control points it draws with
	 * PDI->DrawPoint are proof), so the handles are drawn there instead.
	 *
	 * This only draws; it does not take over the drag. The gizmo still owns hit testing through its own
	 * components (UPrimitiveComponent::LineTraceComponent, which does not involve rendering at all), so
	 * the geometry here deliberately mirrors the gizmo components and the elements asked for: the axes
	 * and plane drawn are exactly the ones the tool can grab.
	 *
	 * @param PDI   draw interface from IToolsContextRenderAPI::GetPrimitiveDrawInterface().
	 * @param View  scene view from IToolsContextRenderAPI::GetSceneView(); supplies the pixel scale and
	 *              the projection test that keep the handles a constant size on screen.
	 */
	void Render(FPrimitiveDrawInterface* PDI, const FSceneView* View) const;

	/**
	 * Tests a world-space ray against the handles this gizmo exposes, in the tool's own screen space.
	 *
	 * The gizmo's own hit testing is not usable here. Drag and render turned out to be two independent
	 * paths, and in this host both of them are dead: Render() proved the scene pass never draws the gizmo
	 * actor (see the class comment on Render), and this was written after a probe showed the components'
	 * LineTraceComponent rejects a ray aimed dead centre at a handle even when every input to it is
	 * provably good - the camera is filled in, the view matrix is orthonormal, the pixel scale is sane and
	 * the handle is nowhere near the view cull. Rather than keep feeding a path that is never driven, the
	 * tool ranks the handles itself, using the exact geometry Render() draws, so what is grabbable and what
	 * is visible are the same by construction.
	 *
	 * @param Ray                world-space ray, from the click or the cursor.
	 * @param InPixelToWorld     world size of one screen pixel at the gizmo, i.e. what
	 *                           GetPixelToWorldScale() reports. An input event carries no view, so the
	 *                           caller supplies the value its Render() measured.
	 */
	FRoadGizmoHit HitTestHandle(const FRay& Ray, double InPixelToWorld) const;

	/**
	 * World size of one screen pixel at the gizmo, for the given view.
	 *
	 * Render() measures this once per frame and the tool caches it, because the drag path has no view of
	 * its own. Returns 0 for a view the handles cannot be sized against, which callers should treat as
	 * "the gizmo is not interactive right now".
	 */
	double GetPixelToWorldScale(const FSceneView* View) const;

	/**
	 * World size of one screen pixel at the gizmo, as measured by the last Render() that drew.
	 *
	 * Render() runs every frame with a real view, and the press-time handle test runs from an input event
	 * that carries none, so the tool reads this kept value instead of measuring again - the frame the user
	 * aimed at is the frame the value was measured in. 0 until the first measurable frame; callers treat
	 * that as "the gizmo is not interactive right now".
	 */
	double GetLastPixelToWorld() const { return LastPixelToWorld; }

	/**
	 * Starts a self-managed drag on a handle and marks it in flight.
	 *
	 * Records the anchor the whole drag measures against: where the target origin was, and where the
	 * cursor was inside the handle's drag subspace (the axis line, or the plane through the origin). Call
	 * it on the press that HitTestHandle accepted, then DragHandle() per move, then EndDrag().
	 */
	void BeginDrag(ERoadGizmoHandle Handle, const FRay& PressRay);

	/** Ends a self-managed drag, running the drag-ended callback once. Safe when no drag is in flight. */
	void EndDrag();

	/**
	 * Moves the target so it tracks the cursor's motion since the drag began.
	 *
	 * The model is the engine's own (UAxisPositionGizmo / UPlanePositionGizmo): the drag subspace is
	 * fixed at the press - the axis line, or the plane, through the drag-start origin - and each frame
	 * measures where the cursor currently sits in that same subspace. The motion is the difference from
	 * where the cursor started, projected onto the directions the handle is allowed to move along, and
	 * the target is placed at start-origin + motion. Because every frame measures against the same
	 * fixed anchor, a step the tool clamps or rejects cannot accumulate drift, and because the motion is
	 * the cursor's *change* rather than its absolute position, the grab offset is cancelled: the point
	 * never jumps to the cursor on the first move.
	 *
	 * The first implementation of the axis case intersected the cursor ray with the plane spanned by the
	 * axis and the camera's view direction, then projected onto the axis. That plane is built to contain
	 * the view direction, so every cursor ray - which is nearly the view direction - is nearly parallel
	 * to it; the ray-plane distance divides by a near-zero denominator and the resulting axis position
	 * swings wildly with small cursor movements. Verified against the live camera: a 100px cursor move
	 * along the axis's own screen projection must move the point ~380 world units, and that method
	 * divided by 0.0017 to get there. The closest-point-on-line measure below has no such denominator
	 * (the same test moves the point 369, i.e. 1:1 tracking), and is exactly what the engine's axis
	 * gizmo does.
	 *
	 * @param Handle      the handle the drag started on, from HitTestHandle.
	 * @param CurrentRay  the ray under the cursor right now.
	 */
	void DragHandle(ERoadGizmoHandle Handle, const FRay& CurrentRay);

	/** True while a drag is in flight, so the tool can hold back work too heavy to do per frame. */
	bool IsDragging() const { return bDragging; }

	/** Runs the drag-ended callback, which is where the deferred geometry rebuild lives. */
	void NotifyDragEnded();

	/**
	 * DIAGNOSTIC (remove when the drag path is confirmed): reports the gizmo actor's editor-side visibility
	 * and whether the renderer has actually drawn it. IsHidden() (checked by Update()'s own probe) reads
	 * only AActor::bHidden, while the editor also has a separate bIsTemporarilyHiddenInEditor flag, and a
	 * component can be visible+registered+in-world yet still never make it into a rendered view. Returns
	 * false when there is no gizmo actor at all.
	 */
	bool GetGizmoActorDiagnostics(bool& bOutEditorHidden, bool& bOutRecentlyRendered) const;

	/**
	 * DIAGNOSTIC (remove when the drag path is confirmed): bit mask of which translate handles the built
	 * actor actually carries - bit0 X arrow, bit1 Y arrow, bit2 XY plane. Elements=22 asks for all three;
	 * a missing bit means the factory did not build that handle at all, which is a different fault from
	 * one that was built and then not drawn.
	 */
	int32 GetGizmoHandleMask() const;

	/**
	 * DIAGNOSTIC (remove when the drag path is confirmed): one line describing the world the gizmo actor
	 * lives in and whether its plane component has reached FScene (SceneProxy != null). A registered
	 * component without a proxy has never been added to the renderer, and an actor in a world that is not
	 * the one the viewport draws can never be rendered no matter how healthy its flags look.
	 */
	FString GetGizmoWorldDiagnostics() const;

private:
	void OnBeginTransformEdit(UTransformProxy* Proxy);
	void OnEndTransformEdit(UTransformProxy* Proxy);

	/** Creates the gizmo on first use. Split out because a tool with nothing selected must not build one. */
	void EnsureGizmo();

	/** The drawing geometry, shared by Render() and HitTestHandle() so the two cannot drift apart. */
	struct FHandleGeometry
	{
		FVector Origin = FVector::ZeroVector;
		FVector AxisX = FVector::XAxisVector;
		FVector AxisY = FVector::YAxisVector;
		/** World size of one screen pixel at the gizmo's depth. The handles are all sized in pixels. */
		double PixelToWorld = 0.0;
		/** 20px gap from the origin, then an 60px shaft: where the arrows start and end. */
		double ArrowStart = 0.0;
		double ArrowEnd = 0.0;
		/** Half-extent of the XY square. */
		double PlaneHalf = 0.0;
	};

	/** Fills in FHandleGeometry for the current target and the given pixel scale. False when unusable. */
	bool GetHandleGeometry(double InPixelToWorld, FHandleGeometry& Out) const;

	UPROPERTY()
	TObjectPtr<UInteractiveTool> OwningTool;

	UPROPERTY()
	TObjectPtr<UTransformProxy> TransformProxy;

	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> TransformGizmo;

	/**
	 * Stand-in component the transform proxy requires.
	 *
	 * UTransformProxy::AddComponentCustom() calls check(Component), so it cannot be handed null, and the
	 * things these tools drag - a road point, a lane segment boundary - are not scene components at all. A
	 * transient, never-registered component satisfies the proxy without the tool touching the user's
	 * actors: the proxy only consults it for IsValid() and Modify(), both disabled by
	 * bModifyComponentOnTransform = false, while the custom get/set pair is what actually reads and writes
	 * the road.
	 */
	UPROPERTY()
	TObjectPtr<USceneComponent> HandleComponent;

	TFunction<FTransform()> GetTransform;
	TFunction<void(const FTransform&)> SetTransform;
	TFunction<void()> OnDragEnded;

	ETransformGizmoSubElements Elements = ETransformGizmoSubElements::TranslateAxisX;
	bool bDragging = false;

	/**
	 * Where the target origin was when the current self-managed drag began.
	 *
	 * Every drag frame places the target at this anchor plus the cursor's motion, so the write callback
	 * receives an absolute transform - the same shape a gizmo-driven drag produces - while the motion
	 * itself stays relative to a fixed reference.
	 */
	FVector DragStartOrigin = FVector::ZeroVector;

	/**
	 * Where the cursor was inside the drag subspace when the drag began.
	 *
	 * For an axis handle this is the point on the axis line nearest the press ray; for a plane handle it
	 * is the press ray's intersection with the drag plane. Both are computed against the subspace
	 * anchored at DragStartOrigin, which is what makes the per-frame difference a pure measure of cursor
	 * motion and cancels the grab offset: the cursor lands somewhere inside the handle, not on its
	 * centre, and that offset is subtracted out instead of snapping the point to the cursor.
	 */
	FVector DragStartCursor = FVector::ZeroVector;

	/**
	 * The drag subspace's directions, captured when BeginDrag() fixed the subspace.
	 *
	 * For a gizmo whose rotation is derived from the data it edits - a boundary tangent, a road frame -
	 * those directions rotate as the drag itself moves the data, and re-reading them per frame would
	 * measure each cursor position in a slightly different subspace than the anchor was taken in, which
	 * reads as drift. The engine's position gizmos get their directions from sources that stand still for
	 * the length of a drag; a data-derived frame does not, so the press-time directions are what the
	 * whole drag measures with.
	 */
	FVector DragStartAxisX = FVector::XAxisVector;
	FVector DragStartAxisY = FVector::YAxisVector;

	/**
	 * World size of one screen pixel at the gizmo, as measured by the last Render() that drew.
	 * See GetLastPixelToWorld(), which hands it to the tools.
	 *
	 * Mutable because Render() is const - drawing does not change the gizmo - and yet it is the one
	 * place that runs every frame with a real view, which makes it the only honest place to measure.
	 */
	mutable double LastPixelToWorld = 0.0;
};
