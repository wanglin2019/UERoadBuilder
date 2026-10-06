// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RoadLineBatchDrawInterface.h"
#include "ToolContextInterfaces.h"

class ULineBatchComponent;

/**
 * Runtime IToolsContextRenderAPI: what a UInteractiveToolsContext hands to UInteractiveTool::Render().
 *
 * The editor gets one of these for free from its tools context. A game does not, so the runtime host
 * supplies its own - this class is the third of the three host-side interfaces the runtime editing host
 * owes the framework (the others being IToolsContextQueriesAPI and IToolsContextTransactionsAPI).
 *
 * It owns the line-batch draw interface and brackets a frame with it:
 *
 *     RenderAPI.BeginFrame(CameraState, EViewInteractionState::Focused);
 *     ToolsContext->Render(&RenderAPI);   // every active tool draws here
 *     RenderAPI.EndFrame();
 *
 * Frames are driven by the game, not by any editor tick, which is why BeginFrame() takes the camera
 * explicitly: there is no viewport client to read it from. The host obtains the scene view and camera
 * state from its own rendering path; the draw interface itself does not depend on either being real,
 * which is what let this be validated before the host exists.
 */
class ROADBUILDERTOOLSRUNTIME_API FRoadRuntimeRenderAPI : public IToolsContextRenderAPI
{
public:
	FRoadRuntimeRenderAPI(const FSceneView* InView, ULineBatchComponent* InLineBatchComponent);

	/** Starts a frame: clears the overlay and records the camera the tools draw against. */
	void BeginFrame(const FViewCameraState& InCameraState, EViewInteractionState InInteractionState);

	/** Publishes the frame's overlay to the line batch component. */
	void EndFrame();

	/** IToolsContextRenderAPI implementation */
	virtual FPrimitiveDrawInterface* GetPrimitiveDrawInterface() override;
	virtual const FSceneView* GetSceneView() override;
	virtual FViewCameraState GetCameraState() override;
	virtual EViewInteractionState GetViewInteractionState() override;

private:
	FRoadLineBatchDrawInterface DrawInterface;

	/** Not owned: the scene view belongs to whoever is rendering the frame. */
	const FSceneView* SceneView = nullptr;

	FViewCameraState CameraState{};
	EViewInteractionState InteractionState = EViewInteractionState::None;
};
