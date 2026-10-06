// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadEdModeToolkit.h"
#include "SRoadEdit.h"
#include "RoadEdMode.h"
#include "RoadToolbars/RoadToolbar.h"
#include "RoadTools/RoadTool.h"

#define LOCTEXT_NAMESPACE "RoadBuilder"

class FRoadBuilderEditorCommands : public TCommands<FRoadBuilderEditorCommands>
{
public:
	FRoadBuilderEditorCommands() : TCommands<FRoadBuilderEditorCommands>(TEXT("RoadBuilderEditor"), LOCTEXT("RoadBuilderEditor", "RoadBuilderEditor"), NAME_None, FAppStyle::GetAppStyleSetName()) {}
	virtual void RegisterCommands() override
	{
		UI_COMMAND(SelectParent, "SelectParent", "SelectParent", EUserInterfaceActionType::Button, FInputChord(EKeys::Escape));
	}
public:
	TSharedPtr<FUICommandInfo> SelectParent;
};

void FRoadEdModeToolkit::Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost)
{
	RoadEdWidget = SNew(SRoadEdit);

	FRoadBuilderEditorCommands::Register();
	ToolkitCommands->MapAction(FRoadBuilderEditorCommands::Get().SelectParent, FExecuteAction::CreateSP(this, &FRoadEdModeToolkit::SelectParent));
	FModeToolkit::Init(InitToolkitHost);
}

class FEdMode* FRoadEdModeToolkit::GetEditorMode() const
{
	return GLevelEditorModeTools().GetActiveMode(FEdModeRoad::GetModeID());
}

TSharedPtr<SWidget> FRoadEdModeToolkit::GetInlineContent() const
{
	return RoadEdWidget;
}

void FRoadEdModeToolkit::GetToolPaletteNames(TArray<FName>& InPaletteName) const
{
	// Panel order equals registry order — this is the single ordered source; do not change it to iterating instances
	for (const FRoadToolbarDesc& Desc : GetRoadToolbarDescs())
		InPaletteName.Add(Desc.PaletteName);
}

FText FRoadEdModeToolkit::GetToolPaletteDisplayName(FName PaletteName) const
{
	return FText::FromString(PaletteName.ToString());
}

void FRoadEdModeToolkit::BuildToolPalette(FName PaletteName, class FToolBarBuilder& ToolBarBuilder)
{
	// Create on demand: FRoadToolbar is a stateless builder (no members, no Init); the instance only lives within this function.
	// Button callbacks capture an ERoadToolType value, not this, so the instance's destruction leaves nothing dangling.
	if (const FRoadToolbarDesc* Desc = FindRoadToolbarDesc(PaletteName))
	{
		const TUniquePtr<FRoadToolbar> Toolbar = Desc->Factory();
		Toolbar->CreateToolbar(ToolBarBuilder);
	}
}

void FRoadEdModeToolkit::OnToolPaletteChanged(FName PaletteName)
{
	// Palette switch -> switch to that palette's default tool (the default comes from the registry; the mode performs the switch)
	if (FEdModeRoad* Mode = FEdModeRoad::Get())
	{
		if (const FRoadToolbarDesc* Desc = FindRoadToolbarDesc(PaletteName))
			Mode->SetCurrentToolByType(Desc->DefaultTool);
	}
}

void FRoadEdModeToolkit::SelectParent()
{
	if (FRoadTool* Tool = FEdModeRoad::Get()->GetCurrentRoadTool())
		Tool->SelectParent();
}
#undef LOCTEXT_NAMESPACE
