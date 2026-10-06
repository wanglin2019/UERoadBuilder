// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadArrayChange.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace
{
	/** Bounds-checked element stride; 0 when the struct type is unusable. */
	int32 GetElementStride(const UScriptStruct* StructType)
	{
		return (StructType != nullptr) ? StructType->GetStructureSize() : 0;
	}

	void* StructAt(TArray<uint8>& Bytes, int32 Index, int32 Stride)
	{
		return (Bytes.Num() >= (Index + 1) * Stride) ? (Bytes.GetData() + Index * Stride) : nullptr;
	}

	const void* StructAt(const TArray<uint8>& Bytes, int32 Index, int32 Stride)
	{
		return (Bytes.Num() >= (Index + 1) * Stride) ? (Bytes.GetData() + Index * Stride) : nullptr;
	}
}

TUniquePtr<FRoadArrayChange> FRoadArrayChange::CaptureBefore(UObject* InOwner, FArrayProperty* InArrayProperty)
{
	if (InOwner == nullptr || InArrayProperty == nullptr)
	{
		return nullptr;
	}

	FStructProperty* StructProperty = CastField<FStructProperty>(InArrayProperty->Inner);
	if (StructProperty == nullptr || StructProperty->Struct == nullptr)
	{
		return nullptr;
	}

	TUniquePtr<FRoadArrayChange> Change(new FRoadArrayChange());
	Change->Owner = InOwner;
	Change->ArrayProperty = InArrayProperty;
	Change->StructType = StructProperty->Struct;
	Change->CaptureInto(Change->Before, Change->BeforeCount);

	return Change;
}

FRoadArrayChange::~FRoadArrayChange()
{
	// The snapshots were built with InitializeStruct(), so they may own heap (TArray members, FText,
	// object references). Destroy them the same way the property system would.
	const int32 Stride = GetElementStride(StructType);
	if (StructType == nullptr || Stride == 0)
	{
		return;
	}

	for (int32 Index = 0; Index < BeforeCount; ++Index)
	{
		if (void* Element = StructAt(Before, Index, Stride))
		{
			StructType->DestroyStruct(Element);
		}
	}
	for (int32 Index = 0; Index < AfterCount; ++Index)
	{
		if (void* Element = StructAt(After, Index, Stride))
		{
			StructType->DestroyStruct(Element);
		}
	}
}

void FRoadArrayChange::CaptureInto(TArray<uint8>& Out, int32& OutCount) const
{
	UObject* OwnerObject = Owner.Get();
	const int32 Stride = GetElementStride(StructType);
	if (OwnerObject == nullptr || ArrayProperty == nullptr || Stride == 0)
	{
		return;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(OwnerObject));
	OutCount = Helper.Num();

	// An empty array is a legitimate snapshot - it is exactly what "before" is on the click that gives a
	// road its first point, and what "after" is when the last one is deleted - but it has no element to
	// construct, so the byte buffer stays empty and GetData() is null. UScriptStruct::InitializeStruct()
	// opens with check(Dest), so it must not be called in that case.
	if (OutCount == 0)
	{
		Out.Reset();
		return;
	}

	Out.SetNumUninitialized(Stride * OutCount);

	// InitializeStruct before copying: CopyScriptStruct assigns into already-constructed members, so an
	// uninitialized destination would corrupt anything with a constructor.
	StructType->InitializeStruct(Out.GetData(), OutCount);
	for (int32 Index = 0; Index < OutCount; ++Index)
	{
		StructType->CopyScriptStruct(Out.GetData() + Index * Stride, Helper.GetRawPtr(Index));
	}
}

void FRoadArrayChange::RestoreFrom(const TArray<uint8>& In, int32 InCount) const
{
	UObject* OwnerObject = Owner.Get();
	const int32 Stride = GetElementStride(StructType);
	if (OwnerObject == nullptr || ArrayProperty == nullptr || Stride == 0)
	{
		return;
	}

	FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(OwnerObject));

	// EmptyAndAddValues() rather than Resize(): it leaves exactly InCount *constructed* elements, which
	// is what copying into them requires, and it makes insertions and removals undo the same way edits do.
	Helper.EmptyAndAddValues(InCount);
	for (int32 Index = 0; Index < InCount; ++Index)
	{
		if (const void* Source = StructAt(In, Index, Stride))
		{
			StructType->CopyScriptStruct(Helper.GetRawPtr(Index), Source);
		}
	}
}

void FRoadArrayChange::CaptureAfter()
{
	CaptureInto(After, AfterCount);
}

void FRoadArrayChange::Apply(UObject* Object)
{
	RestoreFrom(After, AfterCount);
}

void FRoadArrayChange::Revert(UObject* Object)
{
	RestoreFrom(Before, BeforeCount);
}

FString FRoadArrayChange::ToString() const
{
	return FString::Printf(TEXT("FRoadArrayChange(%s, %d -> %d elements)"),
		ArrayProperty != nullptr ? *ArrayProperty->GetName() : TEXT("null"), BeforeCount, AfterCount);
}
