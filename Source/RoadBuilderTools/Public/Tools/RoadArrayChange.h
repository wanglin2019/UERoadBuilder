// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolChange.h"
#include "Templates/UniquePtr.h"

class FArrayProperty;
class UObject;
class UScriptStruct;

/**
 * Undo record for "an array of USTRUCTs on a UObject changed".
 *
 * ITF undo hands the host an FToolCommandChange, and the host's command stack calls Apply()/Revert()
 * later - possibly after the array has been reallocated, reordered, or its owner reloaded. So the
 * record stores the array's *identity* (owner + property) and re-resolves it through the property
 * system on every call, rather than holding pointers into the elements.
 *
 * Snapshotting the whole array rather than a single element is deliberate: it is the primitive that
 * covers every edit a road tool makes to these arrays - change a value, insert an element, remove one -
 * with one implementation and one call site. The arrays involved hold a handful of points each, so the
 * extra copying is irrelevant next to having a single, obviously-correct undo path for all 14 tools.
 *
 * (Legacy contrast: the old tools called Modify() and let the transaction buffer serialize whole
 * objects. An FToolCommandChange has to state exactly what to put back.)
 */
class ROADBUILDERTOOLS_API FRoadArrayChange : public FToolCommandChange
{
public:
	/**
	 * Snapshots the "before" state of the array property on Owner.
	 * @return nullptr when the property or owner cannot be resolved, so callers can skip the undo entry.
	 */
	static TUniquePtr<FRoadArrayChange> CaptureBefore(UObject* Owner, FArrayProperty* ArrayProperty);

	virtual ~FRoadArrayChange() override;

	/** Records the "after" state. Call exactly once, immediately after the edit has been applied. */
	void CaptureAfter();

	/** FToolCommandChange implementation */
	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual FString ToString() const override;

private:
	FRoadArrayChange() = default;

	void CaptureInto(TArray<uint8>& Out, int32& OutCount) const;
	void RestoreFrom(const TArray<uint8>& In, int32 InCount) const;

	/** Held weakly: an undo entry can outlive the object it refers to (actor deleted, level reloaded). */
	TWeakObjectPtr<UObject> Owner;

	/** Raw on purpose: FProperty instances live for the lifetime of the class, not of the object. */
	FArrayProperty* ArrayProperty = nullptr;

	/** Element type, captured at construction. */
	UScriptStruct* StructType = nullptr;

	/** Packed element copies; element i starts at i * StructType->GetStructureSize(). */
	TArray<uint8> Before;
	int32 BeforeCount = 0;

	TArray<uint8> After;
	int32 AfterCount = 0;
};
