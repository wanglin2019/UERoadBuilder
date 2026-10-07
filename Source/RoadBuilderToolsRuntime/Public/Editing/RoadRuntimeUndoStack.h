// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FToolCommandChange;
class UObject;

/**
 * Undo history for the runtime editing host.
 *
 * The editor host gets undo from UnrealEd - IToolsContextTransactionsAPI is implemented there by
 * UEdModeInteractiveToolsContext, which forwards every change into an FScopedTransaction and ends up in
 * the editor's own global undo buffer. A game has none of that, so the runtime host keeps its own.
 *
 * Deliberately not a UObject: the stack is pure bookkeeping over FToolCommandChange, which is a plain
 * non-UObject type, and nothing in the tool layer reaches for it by reflection. It is owned by the
 * subsystem that drives editing, and lives exactly as long as that host does.
 *
 * The unit of history is one transaction, not one change: a single user action emits as many changes as
 * it touches properties, and undoing them has to be atomic. Changes emitted between
 * BeginTransaction()/EndTransaction() therefore collect into one FTransaction and become one undo step,
 * which is the same grouping the editor's transaction system performs. A change emitted with no
 * transaction open is wrapped into a single-change transaction on the spot, mirroring how the tool layer
 * treats a lone EmitArrayChange() as the normal case needing no bracket.
 *
 * Redo is supported because it falls out of the data the stack already holds: a transaction is a list of
 * (target, change) pairs and FToolCommandChange applies and reverts, so the same transaction replays
 * with no extra bookkeeping beyond a cursor.
 */
class ROADBUILDERTOOLSRUNTIME_API FRoadRuntimeUndoStack
{
public:
	FRoadRuntimeUndoStack() = default;
	~FRoadRuntimeUndoStack();

	FRoadRuntimeUndoStack(const FRoadRuntimeUndoStack&) = delete;
	FRoadRuntimeUndoStack& operator=(const FRoadRuntimeUndoStack&) = delete;

	/**
	 * Open a transaction. Reference counted, because the tool layer brackets a multi-property edit with
	 * FRoadUndoTransaction while each individual change inside it opens its own: the inner opens have to
	 * join the outer transaction rather than start a nested step. Every BeginTransaction() must be paired
	 * with an EndTransaction(); the transaction is committed when the last pair closes.
	 */
	void BeginTransaction(const FText& Description);

	/** Close a transaction, committing it to the undo history when the outermost pair closes. */
	void EndTransaction();

	/** Add a change to the open transaction, or to a new single-change one when none is open. */
	void AppendChange(UObject* TargetObject, TUniquePtr<FToolCommandChange> Change, const FText& Description);

	/** Number of committed transactions that can be undone. */
	int32 GetUndoCount() const { return UndoStack.Num() - UndoCursor; }

	/** Number of reverted transactions that can be redone. */
	int32 GetRedoCount() const { return UndoCursor; }

	/** Description of the step undo would revert, or empty text when there is nothing to undo. */
	FText GetUndoDescription() const;

	/** Description of the step redo would re-apply, or empty text when there is nothing to redo. */
	FText GetRedoDescription() const;

	/**
	 * Revert the most recent transaction.
	 * @return the transaction's description when something was undone, or empty text when the stack was
	 *         already at the beginning. The caller uses the non-empty result as "the data changed, rebuild
	 *         the world" - the same signal the editor host gets from FEdMode::PostUndo().
	 */
	FText Undo();

	/** Re-apply the most recently reverted transaction, with the same contract as Undo(). */
	FText Redo();

	/**
	 * Drop the whole history. Called when the edited world is torn down, because the changes hold weak
	 * pointers into it and a change whose target died is silently skipped - which would leave the stack
	 * reporting undo steps that do nothing.
	 */
	void Reset();

private:
	/** One undoable user action: the changes it emitted, in the order they were emitted. */
	struct FTransaction
	{
		FText Description;
		TArray<TPair<TWeakObjectPtr<UObject>, TUniquePtr<FToolCommandChange>>> Changes;
	};

	/** Commit the open transaction, if any, to the undo history. */
	void CommitPendingTransaction();

	/** Every committed transaction, oldest first. Steps at or after UndoCursor are redoable. */
	TArray<TSharedPtr<FTransaction>> UndoStack;

	/**
	 * Number of transactions from the front of UndoStack that have been applied. Everything at or after
	 * it has been reverted and is available to redo, which is why undoing is not a pop: the transaction
	 * has to stay reachable until a new edit discards the redo branch.
	 */
	int32 UndoCursor = 0;

	/** The transaction currently being built; null when no transaction is open. */
	TSharedPtr<FTransaction> PendingTransaction;

	/** Open/close depth, so nested brackets collapse into the outermost transaction. */
	int32 TransactionDepth = 0;

	/** Description of the outermost open transaction, kept across the nested opens. */
	FText PendingDescription;
};
