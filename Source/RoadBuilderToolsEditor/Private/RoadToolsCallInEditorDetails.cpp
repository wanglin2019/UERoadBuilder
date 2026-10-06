// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadToolsCallInEditorDetails.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"

#define LOCTEXT_NAMESPACE "RoadToolsMode"

TSharedRef<IDetailCustomization> FRoadToolsCallInEditorDetails::MakeInstance()
{
	return MakeShareable(new FRoadToolsCallInEditorDetails);
}

void FRoadToolsCallInEditorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	AddCallInEditorMethods(DetailBuilder);
}

void FRoadToolsCallInEditorDetails::AddCallInEditorMethods(IDetailLayoutBuilder& DetailBuilder)
{
	// Metadata tag that lets a function state its sort order within its category.
	static const FName NAME_DisplayPriority("DisplayPriority");

	// Gather first, sort later - the sort needs the whole list.
	TArray<UFunction*, TInlineAllocator<8>> CallInEditorFunctions;
	for (TFieldIterator<UFunction> FunctionIter(DetailBuilder.GetBaseClass(), EFieldIteratorFlags::IncludeSuper); FunctionIter; ++FunctionIter)
	{
		UFunction* TestFunction = *FunctionIter;

		// ParmsSize == 0: a button has nowhere to put arguments, so only no-argument functions qualify.
		if (TestFunction->GetBoolMetaData(FBlueprintMetadata::MD_CallInEditor) && (TestFunction->ParmsSize == 0))
		{
			if (UClass* TestFunctionOwnerClass = TestFunction->GetOwnerClass())
			{
				if (UBlueprint* Blueprint = Cast<UBlueprint>(TestFunctionOwnerClass->ClassGeneratedBy))
				{
					// Blutilities are handled by FEditorUtilityInstanceDetails; drawing them here too
					// would show every button twice.
					if (FBlueprintEditorUtils::IsEditorUtilityBlueprint(Blueprint))
					{
						continue;
					}
				}
			}

			// A function can be reachable through more than one class in the hierarchy being walked.
			const FName FunctionName = TestFunction->GetFName();
			if (!CallInEditorFunctions.FindByPredicate([&FunctionName](const UFunction* Func) { return Func->GetFName() == FunctionName; }))
			{
				CallInEditorFunctions.Add(*FunctionIter);
			}
		}
	}

	if (CallInEditorFunctions.Num() == 0)
	{
		return;
	}

	DetailBuilder.GetObjectsBeingCustomized(/*out*/ SelectedObjectsList);
	if (SelectedObjectsList.Num() == 0)
	{
		return;
	}

	// Category first, then DisplayPriority, then name. Without the final name tie-break the order of
	// equally-ranked buttons would follow the reflected field order, which is not stable across builds.
	CallInEditorFunctions.Sort([](UFunction& A, UFunction& B)
		{
			const int32 CategorySort = A.GetMetaData(FBlueprintMetadata::MD_FunctionCategory).Compare(B.GetMetaData(FBlueprintMetadata::MD_FunctionCategory));
			if (CategorySort != 0)
			{
				return (CategorySort <= 0);
			}
			else
			{
				FString DisplayPriorityAStr = A.GetMetaData(NAME_DisplayPriority);
				int32 DisplayPriorityA = (DisplayPriorityAStr.IsEmpty() ? MAX_int32 : FCString::Atoi(*DisplayPriorityAStr));
				if (DisplayPriorityA == 0 && !FCString::IsNumeric(*DisplayPriorityAStr))
				{
					DisplayPriorityA = MAX_int32;
				}

				FString DisplayPriorityBStr = B.GetMetaData(NAME_DisplayPriority);
				int32 DisplayPriorityB = (DisplayPriorityBStr.IsEmpty() ? MAX_int32 : FCString::Atoi(*DisplayPriorityBStr));
				if (DisplayPriorityB == 0 && !FCString::IsNumeric(*DisplayPriorityBStr))
				{
					DisplayPriorityB = MAX_int32;
				}

				return (DisplayPriorityA == DisplayPriorityB) ? (A.GetName() <= B.GetName()) : (DisplayPriorityA <= DisplayPriorityB);
			}
		});

	struct FCategoryEntry
	{
		FName CategoryName;
		FName RowTag;
		TSharedPtr<SWrapBox> WrapBox;
		FTextBuilder FunctionSearchText;

		FCategoryEntry(FName InCategoryName)
			: CategoryName(InCategoryName)
		{
			WrapBox = SNew(SWrapBox)
				// PreferredSize is a workaround, not a real size: inside a scroll box the wrap box uses
				// its preferred size until the first tick, and a small value there wraps hard and asks
				// the scroll box for too little room. A large value errs the harmless way - the scroll
				// box virtualizes a few extra elements once, then settles.
				.PreferredSize(2000)
				.UseAllottedSize(true);
		}
	};

	// Walk the sorted list once, opening a new entry whenever the category changes, so the strips come
	// out in the same order as the sort.
	FName ActiveCategory;
	TArray<FCategoryEntry, TInlineAllocator<8>> CategoryList;
	for (UFunction* Function : CallInEditorFunctions)
	{
		FName FunctionCategoryName(NAME_Default);
		if (Function->HasMetaData(FBlueprintMetadata::MD_FunctionCategory))
		{
			FunctionCategoryName = FName(*Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory));
		}

		if (FunctionCategoryName != ActiveCategory)
		{
			ActiveCategory = FunctionCategoryName;
			CategoryList.Emplace(FunctionCategoryName);
		}
		FCategoryEntry& CategoryEntry = CategoryList.Last();

		// NameToDisplayString turns "Xodr" into "Xodr" and "ExportXodr" into "Export Xodr".
		const FText ButtonCaption = FText::FromString(FName::NameToDisplayString(*Function->GetName(), false));
		FText FunctionTooltip = Function->GetToolTipText();
		if (FunctionTooltip.IsEmpty())
		{
			FunctionTooltip = FText::FromString(Function->GetName());
		}

		TWeakObjectPtr<UFunction> WeakFunctionPtr(Function);
		CategoryEntry.WrapBox->AddSlot()
			.Padding(0.0f, 0.0f, 5.0f, 3.0f)
			[
				SNew(SButton)
				.Text(ButtonCaption)
				.OnClicked(FOnClicked::CreateSP(this, &FRoadToolsCallInEditorDetails::OnExecuteCallInEditorFunction, WeakFunctionPtr))
				.ToolTipText(FunctionTooltip.IsEmptyOrWhitespace() ? LOCTEXT("CallInEditorTooltip", "Call an event on the selected object(s)") : FunctionTooltip)
			];

		CategoryEntry.RowTag = Function->GetFName();
		CategoryEntry.FunctionSearchText.AppendLine(ButtonCaption);
		CategoryEntry.FunctionSearchText.AppendLine(FunctionTooltip);
	}

	// Add the strips. EditCategory on a name that the object does not otherwise use still creates the
	// category, which is what puts the File panel's lone "Export" strip on screen.
	for (FCategoryEntry& CategoryEntry : CategoryList)
	{
		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(CategoryEntry.CategoryName);
		CategoryBuilder.AddCustomRow(CategoryEntry.FunctionSearchText.ToText())
			.RowTag(CategoryEntry.RowTag)
			[
				CategoryEntry.WrapBox.ToSharedRef()
			];
	}
}

FReply FRoadToolsCallInEditorDetails::OnExecuteCallInEditorFunction(TWeakObjectPtr<UFunction> WeakFunctionPtr)
{
	UFunction* Function = WeakFunctionPtr.Get();
	if (Function == nullptr)
	{
		// The object that owned this function is gone. Nothing to do, and not an error.
		return FReply::Handled();
	}

	// A transaction per click so the button's effect lands on the undo stack as one entry.
	FScopedTransaction Transaction(LOCTEXT("ExecuteCallInEditorMethod", "Call In Editor Action"));

	// CallInEditor is an editor-only entry point; the guard is what lets the function run outside PIE.
	FEditorScriptExecutionGuard ScriptGuard;
	for (TWeakObjectPtr<UObject> SelectedObjectPtr : SelectedObjectsList)
	{
		if (UObject* Object = SelectedObjectPtr.Get())
		{
			Object->ProcessEvent(Function, nullptr);
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
