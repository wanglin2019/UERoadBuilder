// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Editing/RoadRuntimeTransactionsAPI.h"

#include "Editing/RoadRuntimeUndoStack.h"
#include "Engine/World.h"
#include "InteractiveToolChange.h"
#include "RoadLog.h"

FRoadRuntimeTransactionsAPI::FRoadRuntimeTransactionsAPI(UWorld* InWorld, FRoadRuntimeUndoStack* InUndoStack)
	: World(InWorld)
	, UndoStack(InUndoStack)
{
}

void FRoadRuntimeTransactionsAPI::DisplayMessage(const FText& Message, EToolMessageLevel Level)
{
	// A game has no message log to write to. Routing it to the plugin's own log category is what makes the
	// messages reachable at all - a tool that reports "no road under the cursor" through this path would
	// otherwise vanish. Level maps onto the log verbosity the wrapper already tiers.
	switch (Level)
	{
	case EToolMessageLevel::UserMessage:
	case EToolMessageLevel::UserNotification:
		RoadLog_Info(TEXT("tool message: %s"), *Message.ToString());
		break;
	case EToolMessageLevel::UserWarning:
		RoadLog_Warn(TEXT("tool warning: %s"), *Message.ToString());
		break;
	case EToolMessageLevel::UserError:
		RoadLog_Error(TEXT("tool error: %s"), *Message.ToString());
		break;
	case EToolMessageLevel::Internal:
	default:
		// Debug, not Info: the framework's own bookkeeping is not something a game user acts on.
		RoadLog_Debug(TEXT("tool internal: %s"), *Message.ToString());
		break;
	}
}

void FRoadRuntimeTransactionsAPI::PostInvalidation()
{
	// The editor's counterpart calls GEditor->RedrawLevelEditingViewports(). A game redraws its views every
	// frame anyway, so there is no explicit repaint to request - the invalidation the framework is really
	// asking for is "the overlay I batched has changed", and the host re-publishes that batch each frame.
	//
	// Marking render state dirty on every component would be the closest literal translation, but it is
	// both unnecessary (the overlay component is marked dirty by its own Flush()) and very expensive (it
	// walks every component in the world). So this is deliberately a no-op on the runtime side, and the
	// per-frame redraw the game already performs is what makes it correct.
	//
	// Trace: fires per invalidating event, so it belongs at the noisiest level.
	RoadLog_Trace(TEXT("invalidation requested (no-op at runtime; views redraw per frame)"));
}

void FRoadRuntimeTransactionsAPI::BeginUndoTransaction(const FText& Description)
{
	if (UndoStack != nullptr)
	{
		UndoStack->BeginTransaction(Description);
	}
}

void FRoadRuntimeTransactionsAPI::EndUndoTransaction()
{
	if (UndoStack != nullptr)
	{
		UndoStack->EndTransaction();
	}

	// A completed transaction means data changed, so the generated geometry is stale. This is where the
	// runtime host's rebuild differs from the editor's: there is no FEdMode::PostUndo() callback to hang it
	// on, so the host hands this API a handler and the notification happens here.
	if (RebuildRequestHandler)
	{
		RebuildRequestHandler();
	}
}

void FRoadRuntimeTransactionsAPI::AppendChange(UObject* TargetObject, TUniquePtr<FToolCommandChange> Change,
	const FText& Description)
{
	if (UndoStack != nullptr)
	{
		UndoStack->AppendChange(TargetObject, MoveTemp(Change), Description);
	}
}

bool FRoadRuntimeTransactionsAPI::RequestSelectionChange(const FSelectedOjectsChangeList& SelectionChange)
{
	// The selection a road tool cares about is IRoadEditorContext's, not the ITF selection set - the road
	// being edited is one actor, and the tools read it through the context object rather than through
	// ITF. So the framework's selection requests are declined, which is the documented "this context does
	// not support selection changes" answer; nothing in the road tools depends on it.
	//
	// Debug: reported so a future tool that starts relying on it is visible instead of silently ignored.
	RoadLog_Debug(TEXT("RequestSelectionChange declined (%d actors, %d components)"),
		SelectionChange.Actors.Num(), SelectionChange.Components.Num());
	return false;
}
