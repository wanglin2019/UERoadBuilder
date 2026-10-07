// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Input/RoadRuntimeInputBridge.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputRouter.h"
#include "InputState.h"
#include "RoadLog.h"

void FRoadRuntimeInputBridge::Initialize(UInputRouter* InInputRouter, UWorld* InWorld)
{
	InputRouter = InInputRouter;
	World = InWorld;

	// Reset the edge trackers: without this, a bridge re-initialised after a pause would compare the new
	// frame against a stale "was down" and either miss a press or invent a release.
	bLeftWasDown = false;
	bRightWasDown = false;
	bMiddleWasDown = false;
	bHasLastCursorPosition = false;

	if (InputRouter == nullptr)
	{
		// Warning: a bridge with no router posts nowhere. The host is expected to initialise it after the
		// context has come up, so this means the two got out of order.
		RoadLog_Warn(TEXT("input bridge initialised without an input router; it will post nothing"));
	}
}

void FRoadRuntimeInputBridge::Shutdown()
{
	// Do not ForceTerminateAll() here: the caller decides when captures end, because ending them during a
	// teardown that already killed the tools would notify behaviours that no longer exist.
	InputRouter = nullptr;
	World.Reset();
	bHasLastCursorPosition = false;
}

void FRoadRuntimeInputBridge::BuildMouseState(APlayerController* PlayerController, FInputDeviceState& StateOut) const
{
	StateOut.InputDevice = EInputDevices::Mouse;

	// Modifier keys come off the controller so a tool that checks Ctrl/Shift sees the real state. The
	// framework's behaviours read these directly.
	StateOut.SetModifierKeyStates(
		PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift),
		PlayerController->IsInputKeyDown(EKeys::LeftAlt) || PlayerController->IsInputKeyDown(EKeys::RightAlt),
		PlayerController->IsInputKeyDown(EKeys::LeftControl) || PlayerController->IsInputKeyDown(EKeys::RightControl),
		PlayerController->IsInputKeyDown(EKeys::LeftCommand) || PlayerController->IsInputKeyDown(EKeys::RightCommand));

	FMouseInputDeviceState& Mouse = StateOut.Mouse;

	// Button edges. bDown is the instantaneous state; bPressed/bReleased are the edges the framework's
	// behaviours key off, which is why the previous frame's state is remembered.
	const bool bLeftDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bRightDown = PlayerController->IsInputKeyDown(EKeys::RightMouseButton);
	const bool bMiddleDown = PlayerController->IsInputKeyDown(EKeys::MiddleMouseButton);

	Mouse.Left.SetStates(bLeftDown && !bLeftWasDown, bLeftDown, !bLeftDown && bLeftWasDown);
	Mouse.Right.SetStates(bRightDown && !bRightWasDown, bRightDown, !bRightDown && bRightWasDown);
	Mouse.Middle.SetStates(bMiddleDown && !bMiddleWasDown, bMiddleDown, !bMiddleDown && bMiddleWasDown);

	// Cursor position and the ray through it. GetMousePosition() answers in pixels; the deprojection turns
	// that into the world ray every road tool raycasts with.
	float CursorX = 0.0f;
	float CursorY = 0.0f;
	int32 ViewportX = 0;
	int32 ViewportY = 0;
	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;

	if (PlayerController->GetMousePosition(CursorX, CursorY))
	{
		Mouse.Position2D = FVector2D(CursorX, CursorY);

		// Delta2D is the change since the last sample. Zero on the first frame, because there is nothing to
		// difference against and a jump from the origin would read as a huge spurious motion.
		Mouse.Delta2D = bHasLastCursorPosition
			? (Mouse.Position2D - LastCursorPosition)
			: FVector2D::ZeroVector;

		FVector RayOrigin = FVector::ZeroVector;
		FVector RayDirection = FVector::ForwardVector;

		PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
		if (PlayerController->DeprojectScreenPositionToWorld(CursorX, CursorY, RayOrigin, RayDirection))
		{
			Mouse.WorldRay = FRay(RayOrigin, RayDirection);
		}
		else
		{
			// Debug: a deprojection failure means the ray is the default above and every hit test this
			// frame will miss. Verbose because it can happen for a frame or two while a viewport resizes.
			RoadLog_Debug(TEXT("input bridge: deproject failed at (%.0f,%.0f) viewport %dx%d"),
				CursorX, CursorY, ViewportSizeX, ViewportSizeY);
		}
	}
	else
	{
		// No cursor. The framework still gets a coherent state - the ray stays as the last known one - so a
		// captured drag is not cut short by the cursor briefly leaving the viewport.
		Mouse.Position2D = LastCursorPosition;
		Mouse.Delta2D = FVector2D::ZeroVector;

		int32 SizeX = 0;
		int32 SizeY = 0;
		PlayerController->GetViewportSize(SizeX, SizeY);
		const FVector2D ViewportCentre(SizeX * 0.5f, SizeY * 0.5f);

		FVector RayOrigin = FVector::ZeroVector;
		FVector RayDirection = FVector::ForwardVector;
		if (PlayerController->DeprojectScreenPositionToWorld(ViewportCentre.X, ViewportCentre.Y, RayOrigin, RayDirection))
		{
			Mouse.WorldRay = FRay(RayOrigin, RayDirection);
		}
	}

	Mouse.WheelDelta = 0.0f;

	// The remaining unused locals are kept named so the intent of the block above reads clearly; silence
	// the unused warnings for the ones a given build configuration does not touch.
	(void)ViewportX;
	(void)ViewportY;
}

bool FRoadRuntimeInputBridge::PollMouse(APlayerController* PlayerController)
{
	if (InputRouter == nullptr || !World.IsValid() || PlayerController == nullptr)
	{
		return false;
	}

	// Whether anything changed decides whether an event is worth posting. Posting an identical state every
	// frame would drive the framework's hover behaviours continuously, which is both wasteful and wrong:
	// a hover behaviour is for cursor movement, not for the cursor merely existing.
	float CursorX = 0.0f;
	float CursorY = 0.0f;
	const bool bHasCursor = PlayerController->GetMousePosition(CursorX, CursorY);
	const FVector2D CursorPosition = bHasCursor ? FVector2D(CursorX, CursorY) : LastCursorPosition;

	const bool bLeftDown = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bRightDown = PlayerController->IsInputKeyDown(EKeys::RightMouseButton);
	const bool bMiddleDown = PlayerController->IsInputKeyDown(EKeys::MiddleMouseButton);

	const bool bButtonChanged =
		bLeftDown != bLeftWasDown || bRightDown != bRightWasDown || bMiddleDown != bMiddleWasDown;
	const bool bCursorMoved = !bHasLastCursorPosition || !CursorPosition.Equals(LastCursorPosition, 0.01);

	if (!bButtonChanged && !bCursorMoved)
	{
		return false;
	}

	FInputDeviceState State;
	BuildMouseState(PlayerController, State);

	// Both entries are posted: PostInputEvent() drives captures and capture acquisition, while
	// PostHoverInputEvent() is the separate channel the framework keeps for hover-only behaviours. A tool
	// that registers a hover behaviour (the cursor-follow previews) needs the second one; a tool that only
	// clicks is served by the first.
	InputRouter->PostInputEvent(State);
	InputRouter->PostHoverInputEvent(State);

	// Commit the trackers only after a successful post, so a frame where the router is mid-teardown does
	// not consume an edge that would then never be reported.
	bLeftWasDown = bLeftDown;
	bRightWasDown = bRightDown;
	bMiddleWasDown = bMiddleDown;
	LastCursorPosition = CursorPosition;
	bHasLastCursorPosition = bHasCursor;

	// Trace: per-frame, which is exactly the level this belongs at. Turn it on with
	// "log LogRoadBuilder VeryVerbose" when a click is not reaching a tool.
	RoadLog_Trace(TEXT("input posted: L=%d R=%d M=%d pos=(%.0f,%.0f) moved=%d"),
		bLeftDown ? 1 : 0, bRightDown ? 1 : 0, bMiddleDown ? 1 : 0,
		CursorPosition.X, CursorPosition.Y, bCursorMoved ? 1 : 0);

	return true;
}

bool FRoadRuntimeInputBridge::PostKey(const FKey& Key, bool bPressed)
{
	if (InputRouter == nullptr || !Key.IsValid())
	{
		return false;
	}

	FInputDeviceState State;
	BuildKeyState(Key, bPressed, State);
	InputRouter->PostInputEvent(State);

	// Debug: one line per key edge, which is what makes a missing Escape or a swallowed key traceable.
	// Not Trace, because key edges are one-per-action rather than one-per-frame.
	RoadLog_Debug(TEXT("input key %s %s"), *Key.ToString(), bPressed ? TEXT("down") : TEXT("up"));
	return true;
}

void FRoadRuntimeInputBridge::BuildKeyState(const FKey& Key, bool bPressed, FInputDeviceState& StateOut)
{
	StateOut.InputDevice = EInputDevices::Keyboard;

	// A key event describes one key's edge, and the framework supplies no way to report "this key is
	// currently held" beyond that - FKeyboardInputDeviceState holds a single ActiveKey. bDown mirrors
	// bPressed because the event itself is the edge: there is no separate held state to sample.
	StateOut.Keyboard.ActiveKey.Button = Key;
	StateOut.Keyboard.ActiveKey.SetStates(bPressed, bPressed, !bPressed);
}

void FRoadRuntimeInputBridge::ReleaseCaptures()
{
	if (InputRouter != nullptr)
	{
		// ForceTerminateAll() ends every capture and hover, which is the clean slate a host wants when it
		// stops editing or the input mode changes.
		InputRouter->ForceTerminateAll();

		// Info: rare (once per stop), and the state it leaves behind - no capture - is what later input
		// depends on, so it is worth being able to see.
		RoadLog_Info(TEXT("input bridge released all captures"));
	}
}
