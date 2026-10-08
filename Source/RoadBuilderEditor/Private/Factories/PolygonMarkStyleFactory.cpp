// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Factories/PolygonMarkStyleFactory.h"
#include "MarkStyles/PolygonMarkStyle.h"
#include "EditorModeManager.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"
UPolygonMarkStyleFactory::UPolygonMarkStyleFactory(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bText = false;
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
	SupportedClass = UPolygonMarkStyle::StaticClass();
}

UObject* UPolygonMarkStyleFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UPolygonMarkStyle* Object = NewObject<UPolygonMarkStyle>(InParent, InClass, InName, Flags | RF_Transactional);
	return Object;
}

FText FPolygonMarkStyleTypeActions::GetName() const
{
	return LOCTEXT("FPolygonMarkStyleTypeActionsName", "PolygonMark");
}

UClass* FPolygonMarkStyleTypeActions::GetSupportedClass() const
{
	return UPolygonMarkStyle::StaticClass();
}
#undef LOCTEXT_NAMESPACE
