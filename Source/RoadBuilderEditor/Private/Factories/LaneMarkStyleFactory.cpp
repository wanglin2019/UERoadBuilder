// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Factories/LaneMarkStyleFactory.h"
#include "MarkStyles/LaneMarkStyle.h"
#include "EditorModeManager.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"
ULaneMarkStyleFactory::ULaneMarkStyleFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bText = false;
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
	SupportedClass = ULaneMarkStyle::StaticClass();
}

UObject* ULaneMarkStyleFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	ULaneMarkStyle* Object = NewObject<ULaneMarkStyle>(InParent, InClass, InName, Flags | RF_Transactional);
	return Object;
}

FText FLaneMarkStyleTypeActions::GetName() const
{
	return LOCTEXT("FLaneMarkStyleTypeActionsName", "LaneMark");
}

UClass* FLaneMarkStyleTypeActions::GetSupportedClass() const
{
	return ULaneMarkStyle::StaticClass();
}
#undef LOCTEXT_NAMESPACE
