// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Editing/RoadToolsContext.h"

#include "BaseGizmos/TransformGizmoUtil.h"
#include "ContextObjectStore.h"
#include "Editing/RoadRuntimeQueriesAPI.h"
#include "Editing/RoadRuntimeTransactionsAPI.h"
#include "Editing/RoadRuntimeUndoStack.h"
#include "Engine/World.h"
#include "InteractiveToolManager.h"
#include "RoadLog.h"
#include "Rendering/RoadRuntimeRenderAPI.h"

URoadToolsContext::URoadToolsContext()
{
}

bool URoadToolsContext::InitializeRoadEditing(UWorld* InWorld, IRoadEditorContext* InEditorContext)
{
	if (bInitialized)
	{
		// Warning: called twice. Reported rather than asserted - a host re-initialising after a level
		// change is a plausible future path that should not take the process down.
		RoadLog_Warn(TEXT("RoadToolsContext::InitializeRoadEditing called twice; ignoring"));
		return true;
	}

	if (InWorld == nullptr || InEditorContext == nullptr)
	{
		// Error: the caller is the host, and a host with no world has nothing to edit. This is the case
		// where every tool would silently do nothing, so it is stated plainly at the point it happens.
		RoadLog_Error(TEXT("InitializeRoadEditing needs a world and an editor context (world=%d context=%d)"),
			InWorld != nullptr ? 1 : 0, InEditorContext != nullptr ? 1 : 0);
		return false;
	}

	// The undo history has to exist before the Transactions API that writes into it, and both have to
	// exist before Initialize() hands them to the framework - which is the ordering this function's first
	// half exists to guarantee.
	UndoStack = MakeUnique<FRoadRuntimeUndoStack>();
	Queries = MakeUnique<FRoadRuntimeQueriesAPI>(InWorld, InEditorContext);
	Transactions = MakeUnique<FRoadRuntimeTransactionsAPI>(InWorld, UndoStack.Get());

	// Base initialisation: creates the input router, tool manager, gizmo manager, target manager and -
	// the one this host depends on most - the context object store a tool looks the editor context up in.
	UInteractiveToolsContext::Initialize(Queries.Get(), Transactions.Get());

	// The gizmo registration the editor mode performs in its Enter(). Tools that use a transform gizmo
	// reach it through the UE::TransformGizmoUtil helpers, which need a UCombinedTransformGizmoContextObject
	// in the store; without it the first gizmo-based tool asserts on gizmo creation. Idempotent, so this is
	// free to repeat per context.
	//
	// Include is local to the cpp so the gizmo/BasedGizmos dependency stays out of this header.
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(this);

	// Put the host's capability object where every tool looks for it. AddContextObject() is the official
	// ITF seam: a tool calls GetToolManager()->GetContextObjectStore()->FindContext<IRoadEditorContext>()
	// and gets whichever host installed one.
	//
	// The store takes a UObject, so the interface pointer is converted back to the object that implements
	// it - a UINTERFACE's _getUObject() is the supported way to do that, and an interface not implemented
	// by a UObject simply cannot be installed here (it would have no owner to keep alive).
	UObject* ContextObject = InEditorContext->_getUObject();
	if (ContextObject == nullptr)
	{
		// Error: the caller handed an interface not backed by a UObject. It would have no lifetime the
		// store could reason about, and tools would be holding a dangling pointer as soon as the caller's
		// stack unwound.
		RoadLog_Error(TEXT("InitializeRoadEditing: editor context is not backed by a UObject; cannot install"));
		return false;
	}

	UContextObjectStore* ContextStore = (ToolManager != nullptr) ? ToolManager->GetContextObjectStore() : nullptr;
	if (ContextStore == nullptr)
	{
		// Error: without the store no tool can reach the world, and every tool would quietly do nothing.
		RoadLog_Error(TEXT("InitializeRoadEditing: no context object store after Initialize(); host is unusable"));
		return false;
	}

	ContextStore->AddContextObject(ContextObject);

	// Read the interface back exactly the way a tool does. One line that says whether the whole host seam
	// is live - the same check URoadToolsMode::Enter() makes on the editor side.
	const bool bReachable = (ContextStore->FindContext<IRoadEditorContext>() != nullptr);

	bInitialized = true;

	RoadLog_Info(TEXT("runtime host initialised: world=%s reachable=%d toolManager=%d"),
		*InWorld->GetName(), bReachable ? 1 : 0, ToolManager != nullptr ? 1 : 0);

	return bReachable;
}

void URoadToolsContext::ShutdownRoadEditing()
{
	if (!bInitialized)
	{
		return;
	}

	// Tools first: they hold pointers into the world and the context object, so deactivating them before
	// the framework teardown is what keeps a tool's Shutdown() from reaching into a half-dead host.
	DeactivateAllActiveTools(EToolShutdownType::Cancel);

	UInteractiveToolsContext::Shutdown();

	// The APIs are dropped after the framework stops referring to them: Initialize() stored raw pointers,
	// so destroying them first would leave the managers with dangling ones.
	Transactions.Reset();
	Queries.Reset();
	UndoStack.Reset();

	bInitialized = false;

	RoadLog_Info(TEXT("runtime host shut down"));
}

bool URoadToolsContext::RegisterRoadTool(const TCHAR* ToolId, UInteractiveToolBuilder* Builder)
{
	if (ToolId == nullptr || Builder == nullptr)
	{
		RoadLog_Warn(TEXT("RegisterRoadTool with null id or builder"));
		return false;
	}

	if (ToolManager == nullptr)
	{
		// Error: registration before initialisation, which is a host bug and makes every later start fail.
		RoadLog_Error(TEXT("RegisterRoadTool: no tool manager (was InitializeRoadEditing called?)"));
		return false;
	}

	// The tool manager owns the builder from here on.
	ToolManager->RegisterToolType(FString(ToolId), Builder);
	return true;
}

bool URoadToolsContext::StartRoadTool(const TCHAR* ToolId)
{
	if (ToolId == nullptr)
	{
		return false;
	}

	if (!bInitialized)
	{
		RoadLog_Warn(TEXT("StartRoadTool '%s' before initialisation"), ToolId);
		return false;
	}

	// StartTool(), not SelectActiveToolType(): the latter only records the builder and starts nothing, so
	// the host would look like it switched tools while the previous one kept running. Same lesson the
	// editor mode learned; see the note on URoadToolsMode::SelectActiveTool().
	//
	// EToolSide::Left is the side a host's own tools are started on - the same side EToolSide::Mouse names,
	// since the two share an enum value.
	StartTool(EToolSide::Left, FString(ToolId));

	RoadLog_Info(TEXT("runtime start tool '%s'"), ToolId);
	return true;
}

void URoadToolsContext::RenderTools(FRoadRuntimeRenderAPI& RenderAPI)
{
	if (!bInitialized || ToolManager == nullptr)
	{
		return;
	}

	// The tool manager owns the render entry point, not the context: it walks every active tool and calls
	// each one's Render(PDI). The editor reaches it through UEdModeInteractiveToolsContext::Render(), which
	// does the same forwarding - this host goes straight to the manager because it has no mode wrapper to
	// add anything. The RenderAPI brackets the frame; the host called BeginFrame() before this and calls
	// EndFrame() after it returns.
	ToolManager->Render(&RenderAPI);
}
