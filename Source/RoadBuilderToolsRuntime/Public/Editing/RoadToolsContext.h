// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolsContext.h"

#include "RoadToolsContext.generated.h"

class FRoadRuntimeQueriesAPI;
class FRoadRuntimeRenderAPI;
class FRoadRuntimeTransactionsAPI;
class FRoadRuntimeUndoStack;
class IRoadEditorContext;
class UWorld;

/**
 * The runtime editing host's InteractiveToolsContext.
 *
 * UInteractiveToolsContext is the object the whole ITF hangs off: it creates and owns the input router,
 * the tool manager, the gizmo manager, the target manager and the context object store. The editor gets a
 * ready-made subclass (UEdModeInteractiveToolsContext) wired to GEditor; a game has to supply its own, and
 * this is it.
 *
 * Two things are deliberately different from the editor's subclass:
 *
 *   1. The framework object it derives from is the plain UInteractiveToolsContext, not
 *      UEdModeInteractiveToolsContext. The latter's whole purpose is to bridge into FEdMode and the level
 *      editor, which does not exist in a game - depending on it would drag UnrealEd into a packaged build.
 *   2. It owns the Queries / Transactions / Render implementations rather than borrowing them. The editor
 *      subclass implements the first two itself and gets the third from its viewport; here all three are
 *      members, which is what makes the host self-contained.
 *
 * Lifetime is not managed here. A UInteractiveToolsContext has to be Initialise()d with its APIs and
 * Shutdown() before it dies, and the object doing that is the world subsystem - see
 * URoadToolsWorldSubsystem. Keeping creation, initialisation and teardown in one caller is what makes the
 * order provable: the APIs must exist before Initialize(), and the tools must be gone before Shutdown().
 */
UCLASS()
class ROADBUILDERTOOLSRUNTIME_API URoadToolsContext : public UInteractiveToolsContext
{
	GENERATED_BODY()

public:
	URoadToolsContext();

	/**
	 * Bring the context up: build the API implementations, hand them to the framework, and put the host's
	 * context object where the tools look for it.
	 *
	 * Call exactly once, before any tool is registered. RegisterTool() equivalent for this host is
	 * RegisterRoadTool() below.
	 *
	 * @param InWorld          world to edit.
	 * @param InEditorContext  the runtime IRoadEditorContext the tools will find; the caller owns it and
	 *                         must keep it alive for as long as this context.
	 * @return false when the world is unusable, in which case nothing was initialised and the caller
	 *         should tear down rather than proceed.
	 */
	bool InitializeRoadEditing(UWorld* InWorld, IRoadEditorContext* InEditorContext);

	/** Tear the context down. Safe to call on a context that never came up. */
	void ShutdownRoadEditing();

	/**
	 * Register one tool and return its builder, bound to the identifier the palette and the host use to
	 * start it.
	 *
	 * The editor's counterpart is UEdMode::RegisterTool(), which additionally maps a palette command onto
	 * the identifier; a runtime host has no palette command list, so only the identifier half applies.
	 * Called by whichever layer knows the tool set - currently the subsystem, so that the list of tools
	 * lives in exactly one place per host.
	 */
	bool RegisterRoadTool(const TCHAR* ToolId, UInteractiveToolBuilder* Builder);

	/**
	 * Start a tool by identifier, the same path a palette button takes in the editor.
	 *
	 * Asynchronous, like every StartTool(): the identifier is queued and the tool starts on the next tick.
	 * That is a property of the framework, not a shortcut taken here.
	 */
	bool StartRoadTool(const TCHAR* ToolId);

	/** The runtime undo history. Owned here because its lifetime matches this context's edit session. */
	FRoadRuntimeUndoStack* GetUndoStack() const { return UndoStack.Get(); }

	/** Frame the tools draw: called by the host once per rendered frame. */
	void RenderTools(FRoadRuntimeRenderAPI& RenderAPI);

	/** Queries API, so the host can push the camera state in each frame. Never null once initialised. */
	FRoadRuntimeQueriesAPI* GetRoadQueries() const { return Queries.Get(); }

private:
	/** IToolsContextQueriesAPI implementation. Owned. */
	TUniquePtr<FRoadRuntimeQueriesAPI> Queries;

	/** IToolsContextTransactionsAPI implementation. Owned. */
	TUniquePtr<FRoadRuntimeTransactionsAPI> Transactions;

	/** Undo history; the Transactions API writes into it. Owned. */
	TUniquePtr<FRoadRuntimeUndoStack> UndoStack;

	/** Set once InitializeRoadEditing() has run, so ShutdownRoadEditing() is safe to call unconditionally. */
	bool bInitialized = false;
};
