// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

struct FInputDeviceState;
class APlayerController;
class UInputRouter;
class UWorld;

/**
 * Turns a game's mouse and keyboard state into ITF input events.
 *
 * This is the one piece of host plumbing that has no editor counterpart to copy from. In the editor the
 * bridge is UModeManagerInteractiveToolsContext: it wraps FEditorViewportClient's InputKey / InputAxis and
 * posts the result into UInputRouter. A game has no viewport client, no FViewport, and no editor input
 * stack - it has a player controller and a viewport, so the bridge is written against those instead.
 *
 * What it does per frame:
 *
 *   1. reads the cursor position and converts it to a world ray through the player controller's
 *      deprojection,
 *   2. builds an FInputDeviceState describing the mouse (button edges, wheel delta, ray) or the active
 *      keyboard key,
 *   3. posts it through UInputRouter::PostInputEvent(), which either routes it to whatever behaviour is
 *      capturing or asks the tool's behaviours whether they want to start one.
 *
 * Two details are load-bearing and were established against the engine source rather than guessed:
 *
 *   - Press / Release are EDGES. FDeviceButtonState has bPressed / bDown / bReleased and the framework's
 *     behaviours key off bPressed and bReleased, not bDown. So the bridge tracks the previous frame's
 *     button state and only sets bPressed on the frame the button went down, bReleased on the frame it
 *     came up. Reporting bDown as bPressed every frame makes a single click look like a held button and
 *     a drag look like a stream of clicks.
 *   - The ray must be built from the controller's deprojection, not the world origin, or every hit test
 *     misses. GetMousePosition() gives the cursor in pixel space and DeprojectScreenPositionToWorld()
 *     turns that into origin + direction.
 *
 * The bridge does not own the input router: the context does, and it outlives this. It also does not
 * decide whether editing is active - the subsystem calls Poll() only when it is.
 */
class ROADBUILDERTOOLSRUNTIME_API FRoadRuntimeInputBridge
{
public:
	FRoadRuntimeInputBridge() = default;

	/** Bind to the router events are posted into, and the world rays are built against. */
	void Initialize(UInputRouter* InInputRouter, UWorld* InWorld);

	/** Drop the bindings. Safe to call on an uninitialised bridge. */
	void Shutdown();

	/**
	 * Post this frame's mouse state, if it changed.
	 *
	 * @param PlayerController  source of the cursor position; the bridge does nothing without one.
	 * @return true when an event was posted, so the caller can tell "no input" from "input ignored".
	 */
	bool PollMouse(APlayerController* PlayerController);

	/**
	 * Post a keyboard event for one key.
	 *
	 * Called from the game's key handlers rather than polled, because a key event is genuinely an edge:
	 * there is no per-frame key state to sample, and the framework's key behaviours expect one call per
	 * press.
	 *
	 * @param Key     the key that changed.
	 * @param bPressed true for a press, false for a release.
	 */
	bool PostKey(const FKey& Key, bool bPressed);

	/**
	 * Ask the router to drop any active capture.
	 *
	 * Called when editing stops or the input mode changes, so a drag that was in flight when the user
	 * alt-tabbed does not resume mid-gesture with a stale grab point.
	 */
	void ReleaseCaptures();

private:
	/** Build the FInputDeviceState for one mouse frame. */
	void BuildMouseState(APlayerController* PlayerController, FInputDeviceState& StateOut) const;

	/** Build the FInputDeviceState for one key edge. */
	static void BuildKeyState(const FKey& Key, bool bPressed, FInputDeviceState& StateOut);

	/** Not owned: the context owns the router and outlives this bridge. */
	UInputRouter* InputRouter = nullptr;

	/** Weak: the world can be torn down before the host lets go of it. */
	TWeakObjectPtr<UWorld> World;

	/**
	 * Last frame's button state, so press/release can be reported as edges.
	 * Kept per button rather than as one enum because a frame can involve more than one button changing.
	 */
	bool bLeftWasDown = false;
	bool bRightWasDown = false;
	bool bMiddleWasDown = false;

	/** Last frame's cursor position, so Delta2D can be reported. */
	FVector2D LastCursorPosition = FVector2D::ZeroVector;

	/** Whether LastCursorPosition holds a real sample (false until the first PollMouse). */
	bool bHasLastCursorPosition = false;
};
