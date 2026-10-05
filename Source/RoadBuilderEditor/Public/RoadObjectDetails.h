#pragma once
#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

/**
 * Customization for the Settings panel: lays out all CallInEditor blueprint functions of the displayed object as a row of buttons.
 * Previously embedded in SRoadEdit; unrelated to Slate layout, so it lives in its own file.
 */
class FObjectDetails : public IDetailCustomization
{
public:
	// Creates an instance of FObjectDetails
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	// Creates a button strip in each category that contains reflected functions marked as CallInEditor
	void AddCallInEditorMethods(IDetailLayoutBuilder& DetailBuilder);

	// Executes the specified method on the selected objects
	FReply OnExecuteCallInEditorFunction(TWeakObjectPtr<UFunction> WeakFunctionPtr);

private:
	// The list of selected objects, used when invoking a CallInEditor method
	TArray<TWeakObjectPtr<UObject>> SelectedObjectsList;
};
