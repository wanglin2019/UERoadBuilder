// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Editing/RoadRuntimeUndoStack.h"

#include "InteractiveToolChange.h"
#include "RoadBuilderToolsRuntime.h"
#include "RoadLog.h"

FRoadRuntimeUndoStack::~FRoadRuntimeUndoStack()
{
	// The pending transaction is dropped rather than committed: a stack destroyed mid-edit lost its
	// host, so the half-finished action is not something anyone can undo into.
	Reset();
}

void FRoadRuntimeUndoStack::Reset()
{
	UndoStack.Reset();
	UndoCursor = 0;
	PendingTransaction = nullptr;
	TransactionDepth = 0;
	PendingDescription = FText::GetEmpty();
}

void FRoadRuntimeUndoStack::BeginTransaction(const FText& Description)
{
	++TransactionDepth;

	if (TransactionDepth == 1)
	{
		// Only the outermost open creates the transaction; a nested open (the per-change transaction a
		// tool's EmitArrayChange() implicitly requests, inside an FRoadUndoTransaction bracket) joins it.
		PendingTransaction = MakeShared<FTransaction>();
		PendingDescription = Description;
	}
}

void FRoadRuntimeUndoStack::EndTransaction()
{
	if (TransactionDepth <= 0)
	{
		// Unbalanced close. Reported rather than asserted: a tool that mis-pairs its brackets should not
		// take the game down, and the stack is still in a consistent state.
		RoadLog_Warn(TEXT("undo stack: EndTransaction with no open transaction"));
		return;
	}

	--TransactionDepth;
	if (TransactionDepth == 0)
	{
		CommitPendingTransaction();
	}
}

void FRoadRuntimeUndoStack::AppendChange(UObject* TargetObject, TUniquePtr<FToolCommandChange> Change,
	const FText& Description)
{
	if (Change == nullptr)
	{
		return;
	}

	if (PendingTransaction == nullptr)
	{
		// No bracket: this change is a whole action by itself. Open, fill and commit in one go, which is
		// what makes a bare EmitObjectChange() from the tool layer reach the history.
		PendingTransaction = MakeShared<FTransaction>();
		PendingTransaction->Description = Description;
		PendingTransaction->Changes.Emplace(TargetObject, MoveTemp(Change));
		CommitPendingTransaction();
		return;
	}

	// Inside a bracket: the innermost description wins over the outer bracket's, because it names the
	// specific edit that is being made.
	PendingTransaction->Changes.Emplace(TargetObject, MoveTemp(Change));
	if (!Description.IsEmpty())
	{
		PendingTransaction->Description = Description;
	}
}

void FRoadRuntimeUndoStack::CommitPendingTransaction()
{
	TSharedPtr<FTransaction> Transaction = PendingTransaction;
	PendingTransaction = nullptr;

	if (!Transaction.IsValid())
	{
		return;
	}

	// An empty transaction is not a step. It happens whenever a tool opens a bracket and the edit inside
	// turns out to be a no-op (a drag that moved nothing), and committing it would put an act-nothing
	// entry on the undo stack that the user has to press through.
	if (Transaction->Changes.Num() == 0)
	{
		return;
	}

	// A new edit discards the redo branch: the transactions after the cursor describe a future that no
	// longer follows from the current state. This is the standard linear-history rule and the reason the
	// cursor cannot simply be replaced by popping.
	if (UndoCursor < UndoStack.Num())
	{
		UndoStack.RemoveAt(UndoCursor, UndoStack.Num() - UndoCursor);
	}

	UndoStack.Add(MoveTemp(Transaction));
	UndoCursor = UndoStack.Num();

	// Debug: one line per committed step. Verbose-level detail (this fires per user action, not per frame,
	// but it is still per-edit noise once the host works).
	RoadLog_Debug(TEXT("undo: committed '%s' changes=%d depth=%d"),
		*UndoStack.Last()->Description.ToString(), UndoStack.Last()->Changes.Num(), UndoStack.Num());
}

FText FRoadRuntimeUndoStack::GetUndoDescription() const
{
	return GetUndoCount() > 0 ? UndoStack[UndoCursor - 1]->Description : FText::GetEmpty();
}

FText FRoadRuntimeUndoStack::GetRedoDescription() const
{
	return GetRedoCount() > 0 ? UndoStack[UndoCursor]->Description : FText::GetEmpty();
}

FText FRoadRuntimeUndoStack::Undo()
{
	if (GetUndoCount() == 0)
	{
		return FText::GetEmpty();
	}

	TSharedPtr<FTransaction> Transaction = UndoStack[UndoCursor - 1];
	--UndoCursor;

	// Revert in reverse order so a transaction that touched the same object twice unwinds correctly.
	// A dead target is skipped, not an error: undoing an action that spawned an actor and then edited it
	// is a real case, and FToolCommandChangeSequence skips on the same rule.
	for (int32 Index = Transaction->Changes.Num() - 1; Index >= 0; --Index)
	{
		const TPair<TWeakObjectPtr<UObject>, TUniquePtr<FToolCommandChange>>& Entry = Transaction->Changes[Index];
		if (Entry.Key.IsValid())
		{
			Entry.Value->Revert(Entry.Key.Get());
		}
		else
		{
			// Warning: the change could not be reverted because its target died. The history now reports
			// one step fewer than reality, which the user will notice as a missing undo - worth a line.
			RoadLog_Warn(TEXT("undo: target of '%s' is gone, change skipped"),
				*Transaction->Description.ToString());
		}
	}

	// Info: which step was undone, and how much history is left - the pair that says the stack is moving.
	RoadLog_Info(TEXT("undo '%s' remaining=%d"), *Transaction->Description.ToString(), GetUndoCount());

	return Transaction->Description;
}

FText FRoadRuntimeUndoStack::Redo()
{
	if (GetRedoCount() == 0)
	{
		return FText::GetEmpty();
	}

	TSharedPtr<FTransaction> Transaction = UndoStack[UndoCursor];
	++UndoCursor;

	// Forward order here: redo replays the action as it originally happened.
	for (const TPair<TWeakObjectPtr<UObject>, TUniquePtr<FToolCommandChange>>& Entry : Transaction->Changes)
	{
		if (Entry.Key.IsValid())
		{
			Entry.Value->Apply(Entry.Key.Get());
		}
		else
		{
			// Warning: same gap as undo, mirrored.
			RoadLog_Warn(TEXT("redo: target of '%s' is gone, change skipped"),
				*Transaction->Description.ToString());
		}
	}

	// Info: pairs with the undo line.
	RoadLog_Info(TEXT("redo '%s' remaining=%d"), *Transaction->Description.ToString(), GetRedoCount());

	return Transaction->Description;
}
