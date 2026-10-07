// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"

#include "RoadToolsWorldSubsystem.generated.h"

class FRoadRuntimeInputBridge;
class FRoadRuntimeRenderAPI;
class FRoadRuntimeUndoStack;
class SRoadRuntimePropertyPanel;
class URoadRuntimeContextObject;
class URoadToolsContext;
class ULineBatchComponent;

/**
 * Owns the runtime editing host and its lifetime.
 *
 * This is the object that decides when a game is "in road editing mode". It creates the
 * URoadToolsContext, gives it the context object the tools reach the world through, registers the tool
 * set, and drives the two per-frame duties: feeding input into the input router and rendering the active
 * tools' overlays. When it is torn down - level unloaded, subsystem destroyed - it takes all of that down
 * in the reverse order.
 *
 * Why a world subsystem and not a GameInstance one: everything here is per-world. A tool edits one world's
 * roads, the undo history holds weak pointers into that world, and the line batch component hangs off an
 * actor in it. A world subsystem is destroyed with its world, which is exactly the lifetime this needs,
 * and it is created on demand rather than at engine start.
 *
 * The host is off by default. A packaged game that ships the plugin should not start editing roads
 * unprompted, so StartEditing()/StopEditing() are explicit: the game calls them from wherever it wants
 * its own "enter road editor" UI. This also keeps the failure mode obvious - if nothing happens, the game
 * never asked.
 *
 * ShouldCreateSubsystem() is overridden to skip worlds that cannot host an editor: a dedicated server has
 * no viewport, no player controller and no rendering, so the whole subsystem would be dead weight.
 */
UCLASS()
class ROADBUILDERTOOLSRUNTIME_API URoadToolsWorldSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/** USubsystem implementation */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** FTickableGameObject implementation */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual ETickableTickType GetTickableTickType() const override;

	/**
	 * Bring the road editing host up: create the tools context, install the context object, register every
	 * road tool and start the palette's entry tool.
	 *
	 * Idempotent: calling it while already editing is a no-op, so a game can wire it to a key without
	 * guarding.
	 *
	 * @return true when editing is running (either it just started, or it already was).
	 */
	UFUNCTION(BlueprintCallable, Category = "RoadBuilder|Runtime")
	bool StartEditing();

	/** Take the host down, releasing captures and deactivating every tool. Idempotent. */
	UFUNCTION(BlueprintCallable, Category = "RoadBuilder|Runtime")
	void StopEditing();

	/** Whether the host is currently up. */
	UFUNCTION(BlueprintPure, Category = "RoadBuilder|Runtime")
	bool IsEditing() const { return bEditing; }

	/**
	 * Start one tool by its identifier (see the identifiers the editor palettes use).
	 *
	 * Public so a game can drive tool switching from its own UI; a BlueprintCallable wrapper is provided
	 * for that case.
	 */
	bool StartTool(const TCHAR* ToolId);

	/** Undo one step. Returns false when there was nothing to undo. */
	UFUNCTION(BlueprintCallable, Category = "RoadBuilder|Runtime")
	bool UndoEdit();

	/** Redo one step. Returns false when there was nothing to redo. */
	UFUNCTION(BlueprintCallable, Category = "RoadBuilder|Runtime")
	bool RedoEdit();

	/**
	 * The line batch component the tools draw their overlay through.
	 *
	 * Created on demand and hung off an actor this subsystem owns, because a component has to live on one.
	 * A game that wants the overlay drawn by a component of its own can supply it with SetOverlayTarget()
	 * before starting editing.
	 */
	ULineBatchComponent* GetOverlayTarget() const { return OverlayTarget; }

	/** Give the host a component to draw the tool overlay through, instead of the one it would create. */
	void SetOverlayTarget(ULineBatchComponent* InOverlayTarget);

	/**
	 * Called by the game each frame once it has a camera, to hand the host the view the tools draw against.
	 *
	 * A game has no viewport client to read this from, and the render API is frame-scoped, so the camera
	 * has to arrive from the rendering path. A game that never calls this still gets working tools - the
	 * camera state is only used by tools that orient something to the view - so it is not a hard
	 * prerequisite, which is why it is separate from StartEditing().
	 */
	void SetViewState(const struct FViewCameraState& InState);

	/** The tools context, for a game that wants to reach into ITF itself. Null unless editing. */
	URoadToolsContext* GetToolsContext() const { return ToolsContext; }

	/** The runtime undo history, so a game's own UI can label its undo button. Null unless editing. */
	FRoadRuntimeUndoStack* GetUndoStack() const { return ToolsContext != nullptr ? ToolsContext->GetUndoStack() : nullptr; }

	/**
	 * The tool property panel, for a game that wants to place it in its own UI.
	 *
	 * Null until editing has started. The panel follows the active tool by itself - the host points it at
	 * each new tool through this subsystem - so a game only has to decide where it goes: a HUD widget, a
	 * side panel, a UMG viewport slot.
	 *
	 * A game that never calls this still has a fully working editor; it simply has no property UI, which is
	 * the same state the editor is in when its details panel is hidden.
	 */
	TSharedPtr<SRoadRuntimePropertyPanel> GetPropertyPanel() const { return PropertyPanel; }

private:
	/** Create the overlay component and the actor that carries it, if the host has not been given one. */
	void EnsureOverlayTarget();

	/** Register every road tool. Called once per StartEditing(). */
	void RegisterTools();

	/** The tools context; the whole host hangs off it. */
	UPROPERTY(Transient)
	TObjectPtr<URoadToolsContext> ToolsContext;

	/** The IRoadEditorContext implementation the tools find in the context object store. */
	UPROPERTY(Transient)
	TObjectPtr<URoadRuntimeContextObject> RuntimeContext;

	/** Actor that carries the overlay component. Only created when the host made the component itself. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> OverlayActor;

	/** Where the tools' overlay lines are batched. May be game-supplied, in which case OverlayActor is null. */
	UPROPERTY(Transient)
	TObjectPtr<ULineBatchComponent> OverlayTarget;

	/** Input bridge, owned by value: it is stateless plumbing with no UObject needs. */
	TUniquePtr<FRoadRuntimeInputBridge> InputBridge;

	/** Render API, owned by value. Its frame is bracketed per Tick. */
	TUniquePtr<FRoadRuntimeRenderAPI> RenderAPI;

	/** Tool property panel. Built on first StartEditing() and kept for the subsystem's life. */
	TSharedPtr<SRoadRuntimePropertyPanel> PropertyPanel;

	/** Handle for the tool-started/ended subscriptions, cleared on stop. */
	FDelegateHandle ToolStartedHandle;
	FDelegateHandle ToolEndedHandle;

	/** Point the property panel at whichever tool is now active. */
	void RefreshPropertyPanel();

	/** Whether the host is up. */
	bool bEditing = false;
};
