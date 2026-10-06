// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolChange.h"
#include "Templates/UniquePtr.h"

class FProperty;
class UObject;

/**
 * Undo record for "one FProperty of one UObject changed".
 *
 * The single-value counterpart of FRoadArrayChange. That one covers an element of a TArray<FStruct>; this
 * one covers everything else a road tool edits in place - a marking's mesh or UV, for instance, where the
 * value is not an element of anything, so there is no array to snapshot.
 *
 * Like its sibling, the record stores the property's *identity* (owner + property) rather than a pointer
 * into the object, and re-resolves through the property system on every Apply()/Revert(), because an undo
 * entry can outlive the object it refers to (actor deleted, level reloaded, undo stack flushed).
 *
 * The copy goes through the property system's own CopyCompleteValue(), so one implementation covers
 * anything a UPROPERTY can hold - a POD, a struct, a container, an object reference - with no case per
 * type and no way for a type to be silently mishandled.
 */
class ROADBUILDERTOOLS_API FRoadPropertyChange : public FToolCommandChange
{
public:
	/**
	 * Snapshots the "before" value of Property on Owner.
	 * @return nullptr when the property or owner cannot be resolved, so callers can skip the undo entry.
	 */
	static TUniquePtr<FRoadPropertyChange> CaptureBefore(UObject* Owner, FProperty* Property);

	virtual ~FRoadPropertyChange() override;

	/** Records the "after" value. Call exactly once, immediately after the edit has been applied. */
	void CaptureAfter();

	/** FToolCommandChange implementation */
	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override;

private:
	FRoadPropertyChange() = default;

	void CaptureInto(TArray<uint8>& Out, int32& OutSize) const;
	void RestoreFrom(const TArray<uint8>& In, int32 InSize) const;

	/** Held weakly: an undo entry can outlive the object it refers to. */
	TWeakObjectPtr<UObject> Owner;

	/** Raw on purpose: FProperty instances live for the lifetime of the class, not of the object. */
	FProperty* Property = nullptr;

	/** A copy of the value, sized and constructed by the property itself. */
	TArray<uint8> Before;
	int32 BeforeSize = 0;

	TArray<uint8> After;
	int32 AfterSize = 0;
};
