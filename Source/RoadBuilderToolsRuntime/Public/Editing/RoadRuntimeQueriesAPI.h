// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolContextInterfaces.h"

class IRoadEditorContext;
class UWorld;

/**
 * IToolsContextQueriesAPI for the runtime host.
 *
 * The editor host gets one of these free: UEdModeInteractiveToolsContext implements the whole interface,
 * answering with the level editor's selection, its viewport sizes and its coordinate system. A game has
 * none of those, so this class supplies them from the game's own state.
 *
 * The three implementations that matter, and why each is not a stub:
 *
 *   GetCurrentEditingWorld()      - the single most load-bearing query. URoadInteractiveToolBuilder::
 *                                   CanBuildTool() refuses to build a tool without a world, so if this
 *                                   answers null no tool ever starts and the host looks dead.
 *   GetCurrentSelectionState()    - the tools do not read this (they go through IRoadEditorContext, which
 *                                   knows the difference between a road, a ground and a junction); it is
 *                                   answered faithfully anyway because the framework's own gizmo and
 *                                   tool-manager code does call it, and a null world there would be a
 *                                   crash rather than a no-op.
 *   GetCurrentTransformGizmoMode()- Combined, unconditionally, for the same reason the editor mode calls
 *                                   SetForceCombinedGizmoMode(true): a mode left on "Select" otherwise
 *                                   hides the translate handles a road tool asked for.
 *
 * GetStandardMaterial() returns null by design: the only standard material ITF defines is for
 * vertex-coloured meshes, and no road tool draws an ITF mesh - they draw through
 * FRoadRuntimeRenderAPI's line batch. A tool that needs a material owns it as a property instead.
 */
class ROADBUILDERTOOLSRUNTIME_API FRoadRuntimeQueriesAPI : public IToolsContextQueriesAPI
{
public:
	/**
	 * @param InWorld          the world being edited; the answer to GetCurrentEditingWorld().
	 * @param InEditorContext  the runtime IRoadEditorContext, so the selection this reports agrees with
	 *                         the selection the tools see. Not owned.
	 */
	FRoadRuntimeQueriesAPI(UWorld* InWorld, IRoadEditorContext* InEditorContext);

	/** IToolsContextQueriesAPI implementation */
	virtual UWorld* GetCurrentEditingWorld() const override;
	virtual void GetCurrentSelectionState(FToolBuilderState& StateOut) const override;
	virtual void GetCurrentViewState(FViewCameraState& StateOut) const override;
	virtual EToolContextCoordinateSystem GetCurrentCoordinateSystem() const override;
	virtual EToolContextTransformGizmoMode GetCurrentTransformGizmoMode() const override;
	virtual UMaterialInterface* GetStandardMaterial(EStandardToolContextMaterials MaterialType) const override;
	virtual FViewport* GetHoveredViewport() const override;
	virtual FViewport* GetFocusedViewport() const override;

	/**
	 * Update the camera the next GetCurrentViewState() reports.
	 *
	 * The editor reads these off its viewport client; a game has to be told, so the host pushes the
	 * player camera in before the tools run each frame. Without it a tool that orients something to the
	 * view (or sizes a widget by world-to-screen) would be working from a zeroed camera state.
	 */
	void SetViewState(const FViewCameraState& InState);

private:
	/** Weak: the world belongs to the game and may be torn down before the context is. */
	TWeakObjectPtr<UWorld> World;

	/** Not owned; see the constructor. */
	IRoadEditorContext* EditorContext = nullptr;

	/** Camera state as last pushed by the host; zeroed until the first frame. */
	FViewCameraState ViewState{};
};
