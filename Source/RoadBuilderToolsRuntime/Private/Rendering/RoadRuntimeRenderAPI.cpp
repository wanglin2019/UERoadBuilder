// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Rendering/RoadRuntimeRenderAPI.h"

FRoadRuntimeRenderAPI::FRoadRuntimeRenderAPI(const FSceneView* InView, ULineBatchComponent* InLineBatchComponent)
	: DrawInterface(InView, InLineBatchComponent)
	, SceneView(InView)
{
}

void FRoadRuntimeRenderAPI::BeginFrame(const FViewCameraState& InCameraState, EViewInteractionState InInteractionState)
{
	CameraState = InCameraState;
	InteractionState = InInteractionState;

	DrawInterface.Reset();
}

void FRoadRuntimeRenderAPI::EndFrame()
{
	DrawInterface.Flush();
}

FPrimitiveDrawInterface* FRoadRuntimeRenderAPI::GetPrimitiveDrawInterface()
{
	return &DrawInterface;
}

const FSceneView* FRoadRuntimeRenderAPI::GetSceneView()
{
	return SceneView;
}

FViewCameraState FRoadRuntimeRenderAPI::GetCameraState()
{
	return CameraState;
}

EViewInteractionState FRoadRuntimeRenderAPI::GetViewInteractionState()
{
	return InteractionState;
}
