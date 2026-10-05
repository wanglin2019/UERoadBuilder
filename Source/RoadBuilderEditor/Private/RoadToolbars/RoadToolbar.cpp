#include "RoadToolbars/RoadToolbar.h"
#include "RoadEdMode.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"

#include "RoadToolbars/RoadToolbar_File.h"
#include "RoadToolbars/RoadToolbar_Road.h"
#include "RoadToolbars/RoadToolbar_Junction.h"
#include "RoadToolbars/RoadToolbar_Lane.h"
#include "RoadToolbars/RoadToolbar_Marking.h"
#include "RoadToolbars/RoadToolbar_Ground.h"
#include "RoadToolbars/RoadToolbar_Settings.h"

FName PaletteName_File = TEXT("File");
FName PaletteName_Road = TEXT("Road");
FName PaletteName_Junction = TEXT("Junction");
FName PaletteName_Lane = TEXT("Lane");
FName PaletteName_Marking = TEXT("Marking");
FName PaletteName_Ground = TEXT("Ground");
FName PaletteName_Settings = TEXT("Settings");

void FRoadToolbar::AddToolButton(FToolBarBuilder& ToolBarBuilder, ERoadToolType ToolType, const FText& Label, const FText& Tooltip, const FName& IconName)
{
	// The mode is resolved at click time: no pointers captured, callbacks die naturally when the mode/panel is destroyed
	ToolBarBuilder.AddToolBarButton(
		FUIAction(
			FExecuteAction::CreateLambda([ToolType]()
			{
				if (FEdModeRoad* Mode = FEdModeRoad::Get())
					Mode->SetCurrentToolByType(ToolType);
			}),
			FCanExecuteAction::CreateLambda([]() { return true; }),
			FIsActionChecked::CreateLambda([ToolType]()
			{
				FEdModeRoad* Mode = FEdModeRoad::Get();
				return Mode && Mode->GetCurrentToolType() == ToolType;
			})
		),
		NAME_None,
		Label,
		Tooltip,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), IconName),
		EUserInterfaceActionType::ToggleButton
	);
}

const TArray<FRoadToolbarDesc>& GetRoadToolbarDescs()
{
	// Order defines panel display order. Adding a palette only touches this one place.
	// DefaultTool is a tool identity (ERoadToolType), no longer an array index.
	static const TArray<FRoadToolbarDesc> Descs = []()
	{
		TArray<FRoadToolbarDesc> Result =
		{
			{ PaletteName_File,		ERoadToolType::File,			[]() { return TUniquePtr<FRoadToolbar>(new FRoadToolbar_File); } },
			{ PaletteName_Road,		ERoadToolType::RoadPlan,		[]() { return TUniquePtr<FRoadToolbar>(new FRoadToolbar_Road); } },
			{ PaletteName_Junction,	ERoadToolType::JunctionLink,	[]() { return TUniquePtr<FRoadToolbar>(new FRoadToolbar_Junction); } },
			{ PaletteName_Lane,		ERoadToolType::LaneEdit,		[]() { return TUniquePtr<FRoadToolbar>(new FRoadToolbar_Lane); } },
			{ PaletteName_Marking,	ERoadToolType::MarkingLane,		[]() { return TUniquePtr<FRoadToolbar>(new FRoadToolbar_Marking); } },
			{ PaletteName_Ground,	ERoadToolType::GroundEdit,		[]() { return TUniquePtr<FRoadToolbar>(new FRoadToolbar_Ground); } },
			{ PaletteName_Settings,	ERoadToolType::Settings,		[]() { return TUniquePtr<FRoadToolbar>(new FRoadToolbar_Settings); } },
		};

		// Palette names must be unique: OnToolPaletteChanged finds palettes by name only; duplicates would silently break switching
		TSet<FName> SeenNames;
		for (const FRoadToolbarDesc& Desc : Result)
		{
			checkf(!SeenNames.Contains(Desc.PaletteName), TEXT("Duplicate palette name: %s"), *Desc.PaletteName.ToString());
			SeenNames.Add(Desc.PaletteName);
		}
		return Result;
	}();
	return Descs;
}

const FRoadToolbarDesc* FindRoadToolbarDesc(FName PaletteName)
{
	return GetRoadToolbarDescs().FindByPredicate(
		[PaletteName](const FRoadToolbarDesc& Desc) { return Desc.PaletteName == PaletteName; });
}
