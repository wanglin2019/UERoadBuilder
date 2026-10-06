// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadPropertyChange.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

TUniquePtr<FRoadPropertyChange> FRoadPropertyChange::CaptureBefore(UObject* InOwner, FProperty* InProperty)
{
	if (InOwner == nullptr || InProperty == nullptr)
	{
		return nullptr;
	}

	// A static array (ArrayDim > 1) has more than one value per property, which this record does not
	// describe. None of the road data uses one, so refusing is better than silently snapshotting element 0.
	if (InProperty->ArrayDim > 1)
	{
		return nullptr;
	}

	TUniquePtr<FRoadPropertyChange> Change(new FRoadPropertyChange());
	Change->Owner = InOwner;
	Change->Property = InProperty;
	Change->CaptureInto(Change->Before, Change->BeforeSize);

	return Change;
}

FRoadPropertyChange::~FRoadPropertyChange()
{
	if (Property == nullptr)
	{
		return;
	}

	// Free with the property system, which knows how to release whatever the value owns (object
	// references, container storage) - a plain free would leak exactly those.
	if (BeforeSize > 0)
	{
		Property->DestroyValue(Before.GetData());
	}
	if (AfterSize > 0)
	{
		Property->DestroyValue(After.GetData());
	}
}

void FRoadPropertyChange::CaptureInto(TArray<uint8>& Out, int32& OutSize) const
{
	UObject* OwnerObject = Owner.Get();
	if (OwnerObject == nullptr || Property == nullptr)
	{
		return;
	}

	const int32 Size = Property->GetSize();
	Out.SetNumUninitialized(Size);

	// InitializeValue before copying: CopyCompleteValue assigns into an already-constructed value, so
	// handing it raw bytes would run a destructor on garbage later.
	Property->InitializeValue(Out.GetData());
	Property->CopyCompleteValue(Out.GetData(), Property->ContainerPtrToValuePtr<void>(OwnerObject));
	OutSize = Size;
}

void FRoadPropertyChange::RestoreFrom(const TArray<uint8>& In, int32 InSize) const
{
	UObject* OwnerObject = Owner.Get();
	if (OwnerObject == nullptr || Property == nullptr || InSize != Property->GetSize())
	{
		return;
	}

	Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(OwnerObject), In.GetData());
}

void FRoadPropertyChange::CaptureAfter()
{
	CaptureInto(After, AfterSize);
}

void FRoadPropertyChange::Apply(UObject* Object)
{
	RestoreFrom(After, AfterSize);
}

void FRoadPropertyChange::Revert(UObject* Object)
{
	RestoreFrom(Before, BeforeSize);
}

FString FRoadPropertyChange::ToString() const
{
	return FString::Printf(TEXT("FRoadPropertyChange(%s.%s)"),
		Owner.IsValid() ? *Owner->GetName() : TEXT("null"),
		Property != nullptr ? *Property->GetName() : TEXT("null"));
}
