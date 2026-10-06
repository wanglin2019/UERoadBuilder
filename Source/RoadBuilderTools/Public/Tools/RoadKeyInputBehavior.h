// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputBehavior.h"
#include "InputState.h"
#include "Templates/Function.h"

#include "RoadKeyInputBehavior.generated.h"

/**
 * Fires a callback when a single key is pressed.
 *
 * ITF routes mouse input through behaviour targets (IClickBehaviorTarget, IHoverBehaviorTarget, ...) but
 * ships no equivalent for a plain key press: the only keyboard behaviours in the framework are
 * USingleKeyCaptureBehavior and UKeyAsModifierInputBehavior, and both exist to feed a *modifier* into
 * another behaviour rather than to run tool logic. So a tool that needs "Delete removes the selected
 * element" has to derive a behaviour, which is what this is - modelled directly on
 * USingleKeyCaptureBehavior's capture protocol, which is the engine's own template for it.
 *
 * Every legacy tool that overrode FModeTool::InputKey needed exactly this, so it lives in the shared
 * layer rather than in one tool's .cpp.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadKeyInputBehavior : public UInputBehavior
{
	GENERATED_BODY()

public:
	/** @param InKey     key to react to; EKeys::AnyKey matches any key. @param InOnPressed run on press. */
	void Initialize(const FKey& InKey, TFunction<void()> InOnPressed);

	/** UInputBehavior implementation */
	virtual EInputDevices GetSupportedDevices() override;
	virtual FInputCaptureRequest WantsCapture(const FInputDeviceState& Input) override;
	virtual FInputCaptureUpdate BeginCapture(const FInputDeviceState& Input, EInputCaptureSide Side) override;
	virtual FInputCaptureUpdate UpdateCapture(const FInputDeviceState& Input, const FInputCaptureData& Data) override;
	virtual void ForceEndCapture(const FInputCaptureData& Data) override;

private:
	FKey Key;

	/** Held by value: the behaviour owns the callback for as long as the tool keeps the behaviour. */
	TFunction<void()> OnPressed;
};
