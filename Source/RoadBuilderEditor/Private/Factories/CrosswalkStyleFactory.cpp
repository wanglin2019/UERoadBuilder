// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Factories/CrosswalkStyleFactory.h"
#include "MarkStyles/CrosswalkStyle.h"
#include "EditorModeManager.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"
UCrosswalkStyleFactory::UCrosswalkStyleFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bText = false;
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
	SupportedClass = UCrosswalkStyle::StaticClass();
}

UObject* UCrosswalkStyleFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UCrosswalkStyle* Object = NewObject<UCrosswalkStyle>(InParent, InClass, InName, Flags | RF_Transactional);
	return Object;
}

FText FCrosswalkStyleTypeActions::GetName() const
{
	return LOCTEXT("FCrosswalkStyleTypeActionsName", "Crosswalk");
}

UClass* FCrosswalkStyleTypeActions::GetSupportedClass() const
{
	return UCrosswalkStyle::StaticClass();
}
#undef LOCTEXT_NAMESPACE
