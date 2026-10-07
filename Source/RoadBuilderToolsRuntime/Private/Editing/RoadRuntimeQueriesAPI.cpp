// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Editing/RoadRuntimeQueriesAPI.h"

#include "Engine/World.h"
#include "Interface/RoadEditorContext.h"
#include "RoadLog.h"

FRoadRuntimeQueriesAPI::FRoadRuntimeQueriesAPI(UWorld* InWorld, IRoadEditorContext* InEditorContext)
	: World(InWorld)
	, EditorContext(InEditorContext)
{
}

UWorld* FRoadRuntimeQueriesAPI::GetCurrentEditingWorld() const
{
	return World.Get();
}

void FRoadRuntimeQueriesAPI::GetCurrentSelectionState(FToolBuilderState& StateOut) const
{
	// Everything the framework expects to be able to fill in from a context.
	StateOut.World = World.Get();

	// The managers are filled in by UInteractiveToolsContext before it calls into a builder, so they are
	// deliberately left alone here: overwriting them with null would break tools that ask for them.
	//
	// The selected actor, though, comes from the host - and the host is the editor context, which is the
	// one that knows whether the thing the user picked was a road, a ground or a junction. Reporting the
	// selected road here is enough for the framework's own consumers; the road tools themselves query the
	// context object directly and get the type right.
	if (EditorContext != nullptr)
	{
		if (ARoadActor* Road = EditorContext->GetSelectedRoad())
		{
			StateOut.SelectedActors.Add(Road);
		}
	}
}

void FRoadRuntimeQueriesAPI::GetCurrentViewState(FViewCameraState& StateOut) const
{
	StateOut = ViewState;
}

EToolContextCoordinateSystem FRoadRuntimeQueriesAPI::GetCurrentCoordinateSystem() const
{
	// World, matching the editor default. The road tools build their own frame from the road tangent where
	// they need one (see URoadInteractiveTool::ToRoadFrame), so they do not depend on this being anything
	// in particular - it only has to be stable.
	return EToolContextCoordinateSystem::World;
}

EToolContextTransformGizmoMode FRoadRuntimeQueriesAPI::GetCurrentTransformGizmoMode() const
{
	// Combined, always. A UCombinedTransformGizmo derives handle visibility from this query and hides
	// every sub-gizmo the mode does not name, so answering anything else makes a road tool's handles
	// invisible - and an invisible handle fails its own hit test, so the drag never starts. The editor
	// host achieves the same thing with UEditorInteractiveToolsContext::SetForceCombinedGizmoMode(true).
	return EToolContextTransformGizmoMode::Combined;
}

UMaterialInterface* FRoadRuntimeQueriesAPI::GetStandardMaterial(EStandardToolContextMaterials MaterialType) const
{
	// Null by design; see the header. Road tools draw through the line batch, not through ITF meshes.
	return nullptr;
}

FViewport* FRoadRuntimeQueriesAPI::GetHoveredViewport() const
{
	// A game has no FViewport - that type belongs to the editor's viewport framework. Null is the
	// documented "no viewport" answer and every caller in ITF treats it as such.
	return nullptr;
}

FViewport* FRoadRuntimeQueriesAPI::GetFocusedViewport() const
{
	return nullptr;
}

void FRoadRuntimeQueriesAPI::SetViewState(const FViewCameraState& InState)
{
	ViewState = InState;
}
