// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolContextInterfaces.h"

class FRoadRuntimeUndoStack;
class UWorld;

/**
 * IToolsContextTransactionsAPI for the runtime host.
 *
 * The editor host implements this interface inside UEdModeInteractiveToolsContext, which forwards every
 * change into FScopedTransaction and lands them in the editor's global undo buffer. A game has neither
 * UnrealEd nor a global undo buffer, so the runtime host supplies this implementation, backed by
 * FRoadRuntimeUndoStack.
 *
 * This is the seam that makes the tool layer host-neutral: URoadInteractiveTool::EmitArrayChange() calls
 * UInteractiveToolManager::EmitObjectChange(), the manager forwards to the Transactions API it was
 * initialised with, and whichever host is driving gets to decide what undo means. The tool layer never
 * names this class.
 *
 * The selecting host installs it by calling UInteractiveToolsContext::Initialize(QueriesAPI,
 * TransactionsAPI) - the same call UEdMode makes on its own context - so this object's lifetime is tied
 * to the context, not to any single edit session.
 */
class ROADBUILDERTOOLSRUNTIME_API FRoadRuntimeTransactionsAPI : public IToolsContextTransactionsAPI
{
public:
	/**
	 * @param InWorld            the world being edited; used to invalidate that world's views on
	 *                           PostInvalidation().
	 * @param InUndoStack        the history changes are appended to. Not owned; it outlives this API.
	 * @param InOnUndoRedoRequested  invoked after a transaction completes, so the host can rebuild. The
	 *                           Transactions API itself has no way to know the geometry went stale - the
	 *                           host does, and this is how it is told.
	 */
	FRoadRuntimeTransactionsAPI(UWorld* InWorld, FRoadRuntimeUndoStack* InUndoStack);

	/** IToolsContextTransactionsAPI implementation */
	virtual void DisplayMessage(const FText& Message, EToolMessageLevel Level) override;
	virtual void PostInvalidation() override;
	virtual void BeginUndoTransaction(const FText& Description) override;
	virtual void EndUndoTransaction() override;
	virtual void AppendChange(UObject* TargetObject, TUniquePtr<FToolCommandChange> Change,
		const FText& Description) override;
	virtual bool RequestSelectionChange(const FSelectedOjectsChangeList& SelectionChange) override;

	/**
	 * Called by the host after it reverts or re-applies a transaction, so the same rebuild path runs for
	 * undo/redo as for an edit made through a tool.
	 */
	void SetRebuildRequestHandler(TFunction<void()> InHandler) { RebuildRequestHandler = MoveTemp(InHandler); }

private:
	/** Weak: the world belongs to the game and may be torn down before the context is. */
	TWeakObjectPtr<UWorld> World;

	/** Not owned; see the constructor. */
	FRoadRuntimeUndoStack* UndoStack = nullptr;

	/** Invoked when a transaction ends, so the host can regenerate geometry. May be unset. */
	TFunction<void()> RebuildRequestHandler;
};
