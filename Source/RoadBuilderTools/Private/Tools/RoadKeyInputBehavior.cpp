// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadKeyInputBehavior.h"

#include "InputCoreTypes.h"

void URoadKeyInputBehavior::Initialize(const FKey& InKey, TFunction<void()> InOnPressed)
{
	Key = InKey;
	OnPressed = MoveTemp(InOnPressed);
}

EInputDevices URoadKeyInputBehavior::GetSupportedDevices()
{
	return EInputDevices::Keyboard;
}

FInputCaptureRequest URoadKeyInputBehavior::WantsCapture(const FInputDeviceState& Input)
{
	const bool bIsOurKey = (Key == EKeys::AnyKey || Input.Keyboard.ActiveKey.Button == Key);
	if (bIsOurKey && Input.Keyboard.ActiveKey.bPressed)
	{
		return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any);
	}
	return FInputCaptureRequest::Ignore();
}

FInputCaptureUpdate URoadKeyInputBehavior::BeginCapture(const FInputDeviceState& Input, EInputCaptureSide Side)
{
	// Acting on the press rather than the release is what the legacy InputKey handlers did, so a tool
	// migrated from one behaves the same to the finger.
	if (OnPressed)
	{
		OnPressed();
	}
	return FInputCaptureUpdate::Begin(this, EInputCaptureSide::Any);
}

FInputCaptureUpdate URoadKeyInputBehavior::UpdateCapture(const FInputDeviceState& Input, const FInputCaptureData& Data)
{
	// Another key took over the capture: let go rather than swallow it.
	if (Input.Keyboard.ActiveKey.Button != Key)
	{
		return FInputCaptureUpdate::End();
	}
	return Input.Keyboard.ActiveKey.bReleased ? FInputCaptureUpdate::End() : FInputCaptureUpdate::Continue();
}

void URoadKeyInputBehavior::ForceEndCapture(const FInputCaptureData& Data)
{
}
