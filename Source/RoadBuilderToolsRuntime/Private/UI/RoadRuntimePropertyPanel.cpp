// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "UI/RoadRuntimePropertyPanel.h"

#include "InteractiveTool.h"
#include "RoadLog.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RoadRuntimePropertyPanel"

namespace
{
	/** Human-readable name for a property: its display name metadata, else its raw name. */
	FText GetPropertyLabel(const FProperty* Property)
	{
		return Property->HasMetaData(TEXT("DisplayName"))
			? Property->GetMetaDataText(TEXT("DisplayName"))
			: FText::FromString(Property->GetName());
	}

	/**
	 * Whether a property should be editable here.
	 *
	 * EditAnywhere and BlueprintReadWrite are the two markers the road settings use; anything else is
	 * either internal bookkeeping a tool does not want exposed, or is a type this panel cannot render.
	 */
	bool IsEditable(const FProperty* Property)
	{
		if (!Property->HasAnyPropertyFlags(CPF_Edit))
		{
			return false;
		}

		// Transient and editor-only properties are not part of what a game edits.
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditorOnly))
		{
			return false;
		}

		return true;
	}
}

void SRoadRuntimePropertyPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			// Title: which tool the rows below belong to. Without it a panel that silently shows nothing
			// (a tool with no editable properties - File, for instance) is indistinguishable from a broken
			// panel, which was a real diagnosis cost on the editor side.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock)
				.Text(this, &SRoadRuntimePropertyPanel::GetToolTitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(RowContainer, SVerticalBox)
			]
		]
	];
}

void SRoadRuntimePropertyPanel::SetTool(UInteractiveTool* InTool)
{
	Tool = InTool;

	// Rebuild wholesale: the row set is defined by the tool, and tool switches are rare.
	RebuildRows();
}

FText SRoadRuntimePropertyPanel::GetToolTitle() const
{
	const UInteractiveTool* CurrentTool = Tool.Get();
	if (CurrentTool == nullptr)
	{
		return LOCTEXT("NoTool", "No tool active");
	}

	// The class name rather than a display name: a UInteractiveTool carries no friendly name of its own,
	// and the tool class names already read well ("URoadTool_RoadHeight").
	return FText::FromString(CurrentTool->GetClass()->GetName());
}

void SRoadRuntimePropertyPanel::RebuildRows()
{
	if (!RowContainer.IsValid())
	{
		return;
	}

	RowContainer->ClearChildren();

	UInteractiveTool* CurrentTool = Tool.Get();
	if (CurrentTool == nullptr)
	{
		return;
	}

	// bEnabledOnly = false so a property set hidden on one code path still shows when it is the only thing
	// the tool exposes; a panel that shows nothing is worse than one that shows something the tool would
	// not have drawn in the editor.
	TArray<UObject*> PropertySets = CurrentTool->GetToolProperties(/*bEnabledOnly*/ false);

	int32 RowCount = 0;
	for (UObject* PropertySet : PropertySets)
	{
		if (PropertySet == nullptr)
		{
			continue;
		}

		// A separator heading per property set, so a tool with two sets (a settings struct and its own
		// working state) reads as two groups rather than one undifferentiated list.
		if (PropertySets.Num() > 1)
		{
			RowContainer->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(PropertySet->GetClass()->GetName()))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)))
			];
		}

		BuildRowsForObject(PropertySet);
		++RowCount;
	}

	// Debug: how many property sets and whether they produced anything. This is the line that says "the
	// tool exposed nothing" as opposed to "the panel failed to build".
	RoadLog_Debug(TEXT("property panel rebuilt for '%s': sets=%d"),
		*CurrentTool->GetClass()->GetName(), RowCount);
}

void SRoadRuntimePropertyPanel::BuildRowsForObject(UObject* Object)
{
	if (Object == nullptr || !RowContainer.IsValid())
	{
		return;
	}

	// Walk the class chain so inherited properties show too, and honour the tool's own ordering by relying
	// on the reflection order the engine provides.
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		if (Property == nullptr || !IsEditable(Property))
		{
			continue;
		}

		TSharedRef<SWidget> Row = BuildRowForProperty(Object, Property);

		// An unsupported property renders as a single collapsed placeholder - see BuildRowForProperty -
		// which the slot below still adds, so the list keeps its shape and a missing type is visible rather
		// than silently dropped.
		RowContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			Row
		];
	}
}

TSharedRef<SWidget> SRoadRuntimePropertyPanel::BuildRowForProperty(UObject* Owner, FProperty* Property)
{
	const FText Label = GetPropertyLabel(Property);

	// One horizontal row: label on the left at a fixed width, editor filling the rest. The layout is
	// mirrored from what the tools' own panels look like in the editor's details view, so a property is
	// in the same place in both hosts.
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(160.0f)
			[
				SNew(STextBlock)
				.Text(Label)
			]
		];

	// Bools: a checkbox writing straight through to the property.
	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		Row->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Owner, BoolProperty]()
			{
				return BoolProperty->GetPropertyValue_InContainer(Owner)
					? ECheckBoxState::Checked
					: ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Owner, BoolProperty](ECheckBoxState NewState)
			{
				BoolProperty->SetPropertyValue_InContainer(Owner, NewState == ECheckBoxState::Checked);

				// The property is on a config CDO or a tool's working state; marking it dirty is what makes
				// a game's own save pick the change up. There is no transaction here because a checkbox has
				// no drag to coalesce - each toggle is one edit.
				Owner->MarkPackageDirty();
			})
		];

		return Row;
	}

	// Integers and floats: a numeric entry box. Split because FNumericProperty's accessor differs.
	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
	{
		if (NumericProperty->IsFloatingPoint())
		{
			Row->AddSlot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<double>)
				.Value_Lambda([Owner, NumericProperty]()
				{
					return NumericProperty->GetFloatingPointPropertyValue(
						NumericProperty->ContainerPtrToValuePtr<void>(Owner));
				})
				.OnValueCommitted_Lambda([Owner, NumericProperty](double NewValue, ETextCommit::Type)
				{
					NumericProperty->SetFloatingPointPropertyValue(
						NumericProperty->ContainerPtrToValuePtr<void>(Owner), NewValue);
					Owner->MarkPackageDirty();
				})
			];
		}
		else
		{
			Row->AddSlot()
			.FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<int64>)
				.Value_Lambda([Owner, NumericProperty]()
				{
					return static_cast<int64>(NumericProperty->GetSignedIntPropertyValue(
						NumericProperty->ContainerPtrToValuePtr<void>(Owner)));
				})
				.OnValueCommitted_Lambda([Owner, NumericProperty](int64 NewValue, ETextCommit::Type)
				{
					NumericProperty->SetIntPropertyValue(
						NumericProperty->ContainerPtrToValuePtr<void>(Owner), NewValue);
					Owner->MarkPackageDirty();
				})
			];
		}

		return Row;
	}

	// Strings and names: a text box. Names are surfaced as text because that is how the tools' metadata
	// fields read (a shape or style name), and the conversion is lossless for the ASCII names used.
	if (FStrProperty* StrProperty = CastField<FStrProperty>(Property))
	{
		Row->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.Text_Lambda([Owner, StrProperty]()
			{
				return FText::FromString(StrProperty->GetPropertyValue_InContainer(Owner));
			})
			.OnTextCommitted_Lambda([Owner, StrProperty](const FText& NewText, ETextCommit::Type)
			{
				StrProperty->SetPropertyValue_InContainer(Owner, NewText.ToString());
				Owner->MarkPackageDirty();
			})
		];

		return Row;
	}

	if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		Row->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.Text_Lambda([Owner, NameProperty]()
			{
				return FText::FromName(NameProperty->GetPropertyValue_InContainer(Owner));
			})
			.OnTextCommitted_Lambda([Owner, NameProperty](const FText& NewText, ETextCommit::Type)
			{
				NameProperty->SetPropertyValue_InContainer(Owner, FName(*NewText.ToString()));
				Owner->MarkPackageDirty();
			})
		];

		return Row;
	}

	// Enums: shown read-only for now. A proper combo box needs the enum's display names, which the details
	// view gets from the property's metadata; rendering the raw value keeps the panel honest about what it
	// supports instead of offering a control that writes the wrong enum.
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		Row->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([Owner, EnumProperty]()
			{
				const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
				const int64 Value = UnderlyingProperty->GetSignedIntPropertyValue(
					EnumProperty->ContainerPtrToValuePtr<void>(Owner));
				return FText::FromString(EnumProperty->GetEnum()->GetNameStringByValue(Value));
			})
		];

		return Row;
	}

	if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		// A byte property backed by an enum reads as an enum; one that is not is just a number.
		if (const UEnum* Enum = ByteProperty->Enum)
		{
			Row->AddSlot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([Owner, ByteProperty, Enum]()
				{
					const uint8 Value = ByteProperty->GetPropertyValue_InContainer(Owner);
					return FText::FromString(Enum->GetNameStringByValue(Value));
				})
			];

			return Row;
		}
	}

	// Everything else - structs, arrays, objects, maps - is reported as its type name. Editing those
	// properly is what the details view is for; pretending to support them here would produce controls
	// that silently corrupt data, so the panel says what it skipped instead.
	Row->AddSlot()
	.FillWidth(1.0f)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Property->GetCPPType()))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
	];

	return Row;
}

#undef LOCTEXT_NAMESPACE
