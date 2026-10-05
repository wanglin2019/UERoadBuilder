// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "SRoadEdit.h"
#include "RoadEdMode.h"
#include "RoadInspector.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SRoadEdit::Construct(const FArguments& InArgs)
{
	// The panel is owned by FEdModeRoad: we only borrow it here to lay the three views out — no ownership, no lifetime involvement.
	// The views are referenced by the slots below as TSharedRef, so it does not matter whether the panel or this widget dies first.
	FEdModeRoad* Mode = FEdModeRoad::Get();
	checkf(Mode, TEXT("SRoadEdit can only be constructed while FEdModeRoad is active"));
	FRoadInspector* Inspector = Mode->GetInspector();
	checkf(Inspector, TEXT("The inspector panel must be created before the toolkit (see FEdModeRoad::Enter)"));

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			Inspector->GetSettingsWidget()
		]
		+ SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			Inspector->GetObjectWidget()
		]
		+ SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			Inspector->GetStructWidget()
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
