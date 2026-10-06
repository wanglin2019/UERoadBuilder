// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "Math/Ray.h"

#include "RoadInteractiveTool.generated.h"

class ARoadActor;
class ARoadScene;
class AGroundActor;
class AJunctionActor;
class FPrimitiveDrawInterface;
class FRoadArrayChange;
class FRoadPropertyChange;
class IRoadEditorContext;
class UInputBehavior;
class UInteractiveToolManager;
class URoadBoundary;
class URoadCurve;
class URoadLane;
class UWorld;
struct FInputDeviceRay;
struct FInputRayHit;
struct FPolyline;

/**
 * Drawing style shared by every road tool.
 *
 * Lifted from the legacy FRoadTool constants so the migrated tools keep looking identical, and so that
 * a colour change lands in one place instead of fourteen. Names are kept verbatim on purpose: diffing
 * the legacy tool against its ITF replacement then shows only the functional differences.
 */
namespace RoadToolStyle
{
	inline const FLinearColor Color_Road = FLinearColor(0.5f, 0.5f, 1.0f);
	inline const FLinearColor Color_Line = FLinearColor(0.0f, 0.5f, 0.0f);
	inline const FLinearColor Color_Grey = FLinearColor(0.5f, 0.5f, 0.5f);
	inline const FLinearColor Color_Select = FLinearColor(1.0f, 0.0f, 0.0f);
	inline const FLinearColor Color_HandleX = FLinearColor(1.0f, 0.0f, 0.0f);
	inline const FLinearColor Color_HandleY = FLinearColor(0.0f, 1.0f, 0.0f);
	inline const FLinearColor Color_HandleXY = FLinearColor(0.0f, 0.0f, 1.0f);
	inline constexpr float Size_Point = 16.0f;
	inline constexpr float Thickness_Road = 2.0f;
	inline constexpr float Thickness_Line = 1.0f;
	inline constexpr float DepthBias_Select = 10.0f;
}

/**
 * Base builder for the road tools.
 *
 * Only the precondition shared by every road tool lives here: a tool raycasts into the world, so a host
 * without a world (early start-up, an asset-only editor, a runtime host before the level is up) must not
 * build one. Concrete builders implement BuildTool() and hand the world to the tool through
 * URoadInteractiveTool::SetEditingWorld().
 */
UCLASS(Abstract)
class ROADBUILDERTOOLS_API URoadInteractiveToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Base class for the road tools.
 *
 * It carries what every tool needs regardless of which host is driving it, and nothing else:
 * where the host capabilities come from (IRoadEditorContext), which world to edit, and the handful of
 * conventions the tools share for reaching and drawing the road data. Everything host-specific - input
 * plumbing, rendering, property panels, undo - is supplied by the host through ITF, which is what keeps
 * this class free of editor types.
 *
 * The host accessors below deliberately mirror the legacy FRoadTool helpers (GetSelectedRoad, DrawRoads,
 * DrawCurve) rather than inventing a new vocabulary: those five helpers were the only part of FRoadTool
 * that every one of the 14 tools actually used, so they are the part worth carrying over.
 */
UCLASS(Abstract)
class ROADBUILDERTOOLS_API URoadInteractiveTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	/**
	 * Host capabilities, injected by the active host through UContextObjectStore.
	 * May return null when a host forgot to inject its context object; callers must tolerate that
	 * rather than assume a host.
	 */
	IRoadEditorContext* GetRoadEditorContext() const;

	/**
	 * World this tool operates on. The host context wins when available, otherwise the world the
	 * builder was given in BuildTool() is used as a fallback.
	 */
	UWorld* GetEditingWorld() const;

	/**
	 * Fallback world for GetEditingWorld(). Public because the caller is the tool's builder in
	 * BuildTool(), not the tool itself; tools should treat the world as read-only.
	 */
	void SetEditingWorld(UWorld* InWorld);

	/** Road the host currently treats as selected, or null. Tolerates a host with no context object. */
	ARoadActor* GetSelectedRoad() const;

	/**
	 * Asks the host to make this road the selected one; a null request clears the selection.
	 *
	 * Needed by the tools that select a road themselves rather than only reading what the host already
	 * had: SelectRoadUnderRay() below is the common case, but a tool that drops a road from its selection
	 * when a click lands on empty space needs the bare setter.
	 */
	void SetSelectedRoad(ARoadActor* Road) const;

	/** Road network of the edited world, or null. Tolerates a host with no context object. */
	ARoadScene* GetRoadScene() const;

	/** Ground the host currently treats as selected, or null. Tolerates a host with no context object. */
	AGroundActor* GetSelectedGround() const;

	/** Junction the host currently treats as selected, or null. Tolerates a host with no context object. */
	AJunctionActor* GetSelectedJunction() const;

	/** Asks the host to make this ground the selected one; a null request clears the selection. */
	void SetSelectedGround(AGroundActor* Ground) const;

	/** Asks the host to make this junction the selected one; a null request clears the selection. */
	void SetSelectedJunction(AJunctionActor* Junction) const;

	/** Redraws the host's view. Cheap, but only needed when the tool's own drawing changed. */
	void RequestRedraw() const;

	/** Asks the host to regenerate road geometry after a data change. Expensive; never per-frame. */
	void RequestRebuild() const;

	/**
	 * World-space ray test against the edited level.
	 *
	 * The legacy FRoadTool::LineTrace equivalent, including its sentinel: a miss returns
	 * FVector(WORLD_MAX, WORLD_MAX, WORLD_MAX), and callers test Position.X < WORLD_MAX to tell a miss
	 * apart from a real position. A tool that acts at a world position (chop, carve) needs that
	 * distinction, so it is preserved rather than substituted with a bool out-parameter.
	 */
	FVector LineTrace(const FRay& Ray, AActor* IgnoredActor = nullptr) const;

	/**
	 * Picks the road under the ray and hands it to the host as the selection.
	 *
	 * The legacy FRoadTool::HandleClickRoad equivalent, and the reason it lives here: six tools begin by
	 * resolving "which road did the user mean", and the legacy base class carried the same helper for the
	 * same reason. Returns the picked road, or null when the ray missed every road - in which case the
	 * selection is deliberately left alone, as the legacy helper also did.
	 */
	ARoadActor* SelectRoadUnderRay(const FRay& Ray) const;

	/**
	 * Registers the left-button click binding.
	 *
	 * The left button only, and deliberately. A behaviour claims the press it accepts, and the editor takes
	 * a claimed button away from the viewport for the whole drag: UModeManagerInteractiveToolsContext::
	 * InputKey routes the press to the tools context instead of letting the viewport start tracking, and
	 * CapturedMouseMove then routes the movement to the tool rather than to the camera. Since
	 * IsHitByRoadClick() accepts anywhere, a right-button behaviour would cost the editor right-drag orbit
	 * and WASD for as long as a tool is active - too much to pay, when right clicks reach the tool through
	 * the host's own click path anyway (see HandleViewportClick()).
	 */
	void AddClickBehavior();

	/**
	 * Called for a click that reached the host instead of one of this tool's input behaviours.
	 *
	 * The right button only ever arrives this way, which is what keeps right-drag free for the camera.
	 */
	virtual bool HandleViewportClick(const FRay& WorldRay, bool bRightButton);

	/** Click landed on a bound button. bRightButton tells the two buttons apart. */
	virtual void OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton);

	/**
	 * Whether the left-button behaviour wants the click at all. Defaults to accepting anywhere, because ITF
	 * has no hit proxies to test against and the tool decides what was hit itself in OnRoadClicked().
	 */
	virtual FInputRayHit IsHitByRoadClick(const FInputDeviceRay& ClickPos);

	/**
	 * Registers a hover behaviour that keeps GetLastCursorRay() up to date.
	 *
	 * Needed by tools that preview something under the cursor as it moves. IToolsContextRenderAPI carries
	 * the camera and the interaction state but not the cursor ray, so the ray has to arrive through the
	 * input path and be remembered between the input event and the render pass.
	 */
	void AddHoverBehavior();

	/**
	 * Registers the Escape binding that steps the selection up one level, and the tool state a step needs.
	 *
	 * The legacy FModeTool did this in InputKey(Escape) plus a toolkit command; here it is one key
	 * behaviour, which is what the shared URoadKeyInputBehavior is for. Only the tools with a multi-level
	 * selection call it - the same set that overrode the legacy InputKey.
	 */
	void AddSelectParentBehavior();

	/**
	 * Steps the selection up one level: road -> owning junction -> nothing.
	 *
	 * The legacy FRoadTool::SelectParent, kept as a virtual so a tool whose selection has more levels can
	 * extend it. The base walks the two levels every legacy tool shared and clears the tool's own
	 * sub-selection, which a tool does by overriding and calling this first.
	 */
	virtual void SelectParent();

	/**
	 * Registers the left-button click-and-drag binding a point-editing tool uses to drag its gizmo handles.
	 *
	 * This exists because the gizmo's own drag never runs here. A UCombinedTransformGizmo normally takes
	 * the press itself - its sub-gizmos register with the input router at DEFAULT_GIZMO_PRIORITY (50) and
	 * outrank a tool behaviour at DEFAULT_TOOL_PRIORITY (100) - but in this host it does not: a probe that
	 * reproduces the components' own LineTraceComponent by hand finds no hit even when the ray is aimed
	 * dead centre at a handle, with a filled-in view context, an orthonormal view matrix and a sane pixel
	 * scale. That is the same shape as the render fault (see URoadPointGizmo::Render): the host simply
	 * does not drive the gizmo's own machinery. So, like the drawing, the drag is taken over by hand.
	 *
	 * Priority is what makes this safe. Both behaviours live on the same tool, and the router sorts
	 * requests by (priority, then hit depth) - InputBehavior.cpp:9-19 - so a drag behaviour asked for at a
	 * lower priority number than DEFAULT_TOOL_PRIORITY is consulted first and wins the press whenever it
	 * reports a hit on a handle. When it does not, it declines and the plain click behaviour still gets
	 * the click, which is what keeps selection working exactly as before.
	 */
	void AddDragBehavior();

	/**
	 * Whether a drag can begin at this press, and how far away the thing grabbed was.
	 *
	 * A tool that drags a gizmo handle answers with the hit its own handle test produced. The default
	 * accepts nothing, so a tool that never calls AddDragBehavior() is unaffected.
	 */
	virtual FInputRayHit CanBeginRoadDrag(const FInputDeviceRay& PressPos);

	/** A drag is in flight and the cursor has moved. Apply the motion. */
	virtual void OnRoadDragged(const FInputDeviceRay& DragPos);

	/** The drag ended; commit whatever the motion accumulated. */
	virtual void OnRoadDragEnded();

	/** Last cursor ray seen while hovering. False until the cursor has entered the viewport, or after it left. */
	bool GetLastCursorRay(FRay& OutRay) const;

	/**
	 * DIAGNOSTIC (remove when the click path is confirmed): logs every behaviour registration.
	 *
	 * Every tool registers its behaviours inside Setup(), so this one override reports, once per tool,
	 * that Setup() ran, which tool it was, and what the host looked like at that moment. It is the only
	 * hook that fires during Setup for all fourteen tools without touching any of them.
	 */
	virtual void AddInputBehavior(UInputBehavior* Behavior, void* Source = nullptr) override;

	/**
	 * Draws every road of the edited scene, with the selected one highlighted.
	 *
	 * The legacy FRoadTool::DrawRoads equivalent. bDrawLinks adds the curves a junction owns - the ramps
	 * between its gates - which the tools that let the user pick any road need, because a ramp is a road
	 * whose visible extent is not only its own centreline. Note that picking follows the centreline only:
	 * the legacy link pass also put a hit proxy on the ramp curve itself, and a ray test against the road's
	 * own curve is a strictly narrower offer. Widening it would mean teaching RoadPicking about junctions.
	 */
	void DrawRoads(FPrimitiveDrawInterface* PDI, bool bDrawLinks = false) const;

	/** Draws a polyline as a series of segments. The legacy FRoadTool::DrawCurve equivalent. */
	static void DrawCurve(FPrimitiveDrawInterface* PDI, const FPolyline& Curve, const FLinearColor& Color,
		float Thickness, float DepthBias = 0.0f);

	/** Draws the cross-line of Lane at Dist, right boundary to left boundary. The legacy DrawDivider(). */
	static void DrawDivider(FPrimitiveDrawInterface* PDI, const URoadLane* Lane, double Dist,
		const FLinearColor& Color);

	/** Draws a point at Dist along Curve. The legacy DrawPoint(). */
	static void DrawPoint(FPrimitiveDrawInterface* PDI, URoadCurve* Curve, double Dist,
		const FLinearColor& Color);

	/**
	 * Splits a world-space delta into the frame the road tools are written in: X along the road, Y across
	 * it.
	 *
	 * The legacy tools got this from the editor's custom drawing coordinate system, which they set to the
	 * road tangent so that "drag X" meant "move along the road" and "drag Y" meant "widen". A gizmo reports
	 * an absolute position rather than a delta, so a tool that stores a station takes the difference
	 * between where the gizmo wants the point and where the point currently is, and hands that difference
	 * here. Taking the difference against the data - never against the previous event - is what keeps a
	 * clamped edit from drifting the gizmo away from the point it is editing.
	 */
	static FVector2D ToRoadFrame(ARoadActor* Road, double Dist, const FVector& Delta);

	/**
	 * Records an edit to an array of USTRUCTs on Owner as one undo entry.
	 *
	 * The host's AppendChange opens a transaction of its own, so a value edit needs no bracket around it
	 * and this is the normal call shape. A logical edit that has to touch two properties at once (a
	 * marking curve closing touches both its point array and its closed-loop flag) can instead be wrapped
	 * in one FRoadUndoTransaction: the host's transaction machinery is reference counted, so the inner
	 * transaction the host opens per change joins the outer one rather than nesting a second undone step.
	 *
	 * Structural edits - the ones that create or destroy objects - go through FRoadUndoTransaction alone,
	 * because a change to a single object cannot describe them.
	 */
	void EmitArrayChange(UObject* Owner, TUniquePtr<FRoadArrayChange> Change, const FText& Description);

	/** The single-value counterpart of EmitArrayChange(), for a property that is not an array element. */
	void EmitPropertyChange(UObject* Owner, TUniquePtr<FRoadPropertyChange> Change, const FText& Description);

private:
	/** Fallback for GetEditingWorld() when no IRoadEditorContext is injected. */
	TWeakObjectPtr<UWorld> EditingWorld;

	/** Cursor ray captured by AddHoverBehavior(); meaningful only while bHasCursorRay is true. */
	FRay LastCursorRay;

	/** Whether LastCursorRay holds a ray the cursor produced. */
	bool bHasCursorRay = false;
};

/**
 * RAII bracket around a host undo transaction.
 *
 * Needed for the edits that spawn or destroy actors - road chop, join, split, ground join - because those
 * cannot be expressed as an FToolCommandChange on a single object, which is all EmitObjectChange() can
 * record. The legacy tools reached for FScopedTransaction in exactly these places and nowhere else, and
 * this is the host-neutral counterpart: the host maps it onto whatever its own undo stack is.
 *
 * It also serves as the coalescing bracket for a value edit that spans more than one property: the host
 * opens a reference-counted transaction per emitted change, so changes emitted inside this bracket all
 * land in the same undo step. Value edits that touch a single property need no bracket at all.
 */
class ROADBUILDERTOOLS_API FRoadUndoTransaction
{
public:
	FRoadUndoTransaction(UInteractiveToolManager* InToolManager, const FText& Description);
	~FRoadUndoTransaction();

	FRoadUndoTransaction(const FRoadUndoTransaction&) = delete;
	FRoadUndoTransaction& operator=(const FRoadUndoTransaction&) = delete;

private:
	/** Null when the tool has no manager; the bracket then does nothing rather than crash. */
	UInteractiveToolManager* ToolManager = nullptr;
};

