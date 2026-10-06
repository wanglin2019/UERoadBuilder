// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class IDetailLayoutBuilder;

/**
 * Lays out an object's CallInEditor functions as a row of buttons.
 *
 * The engine's details view renders FProperty only - it never looks at UFUNCTION, so an object whose
 * entire interface is CallInEditor functions (USettings_File, which has nothing but Xodr()) comes out
 * as an empty panel with no way to tell why. The functions have to be collected and drawn by hand,
 * which is what this customization does.
 *
 * This is the same job the legacy layer's FObjectDetails does, and the two are deliberately separate
 * copies: the legacy module is frozen and is not a dependency of this one, so sharing the class would
 * mean either coupling the new module to the old one or adding a module for a single helper. If the
 * two ever need to diverge - this one, for instance, could start honoring a per-mode function filter -
 * the copy is what makes that possible.
 *
 * Register it on any details view that displays an object with CallInEditor functions:
 *   View->RegisterInstancedCustomPropertyLayout(UObject::StaticClass(),
 *       FOnGetDetailCustomizationInstance::CreateStatic(&FRoadToolsCallInEditorDetails::MakeInstance));
 */
class FRoadToolsCallInEditorDetails : public IDetailCustomization
{
public:
	/** Creates an instance of FRoadToolsCallInEditorDetails. */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	/**
	 * Adds a button strip to each category that contains reflected functions marked CallInEditor.
	 *
	 * Grouping by the function's Category metadata is what keeps the strips in the same categories as
	 * the properties around them, so a button lands next to the settings it acts on.
	 */
	void AddCallInEditorMethods(IDetailLayoutBuilder& DetailBuilder);

	/** Executes the given function on every customized object. */
	FReply OnExecuteCallInEditorFunction(TWeakObjectPtr<UFunction> WeakFunctionPtr);

private:
	/**
	 * The objects being customized, kept for invocation.
	 *
	 * Weak, and captured at customization time: the details view can outlive the objects it was
	 * pointed at (a tool shutting down, an actor deleted), and a stale strong reference here would
	 * keep a dead object alive and let a click reach it.
	 */
	TArray<TWeakObjectPtr<UObject>> SelectedObjectsList;
};
