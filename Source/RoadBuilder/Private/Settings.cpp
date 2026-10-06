// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Settings.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RoadActor.h"
#include "RoadScene.h"
#if WITH_EDITOR
#include "Editor.h"
// Kept alongside the selection-based helpers below: the legacy FEdModeRoad still ships in this plugin,
// and its own selected road / scene is what its panel buttons should act on.
#include "RoadBuilderEditor/Public/RoadEdMode.h"
void USettings_Base::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty)
		SaveConfig();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

USettings_Global::USettings_Global(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultDrivingShape = LoadObject<ULaneShape>(nullptr, TEXT("/RoadBuilder/LaneShapes/Driving.Driving"));
	DefaultSidewalkShape = LoadObject<ULaneShape>(nullptr, TEXT("/RoadBuilder/LaneShapes/Sidewalk.Sidewalk"));
	DefaultMedianShape = LoadObject<ULaneShape>(nullptr, TEXT("/RoadBuilder/LaneShapes/Median.Median"));
	DefaultDashStyle = LoadObject<ULaneMarkStyle>(nullptr, TEXT("/RoadBuilder/MarkStyles/LaneMark/WhiteDash.WhiteDash"));
	DefaultSolidStyle = LoadObject<ULaneMarkStyle>(nullptr, TEXT("/RoadBuilder/MarkStyles/LaneMark/WhiteSolid.WhiteSolid"));
	DefaultGroundMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("MaterialInstanceConstant'/Game/CityPark/Materials/Ground/MI_Ground02.MI_Ground02'"));
	DefaultGoreMarking = LoadObject<UPolygonMarkStyle>(nullptr, TEXT("/RoadBuilder/MarkStyles/PolygonMark/ChevronRegion.ChevronRegion'"));
	BuildJunctions = 1;
	BuildProps = 1;
	DisplayGateRadianPoints = 0;
}

USettings_OSM::USettings_OSM(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ConnectRoads = 1;
}
#if WITH_EDITOR
namespace
{
	/**
	 * The road the panel actions apply to, whichever editor mode is driving the edit.
	 *
	 * The legacy FEdModeRoad keeps its own selected road, while URoadToolsMode drives the editor's actor
	 * selection instead. Asking both is what lets these buttons work in either mode: a bare
	 * FEdModeRoad::Get() dereferences a null mode as soon as the new mode is the active one, which made
	 * the property panel a crash waiting for a click. The mode is still looked up through the registry,
	 * so this stays correct whether or not the legacy mode is the active one.
	 */
	ARoadActor* GetEditingRoad()
	{
		if (FEdModeRoad* LegacyMode = FEdModeRoad::Get())
		{
			return LegacyMode->SelectedRoad;
		}

		if (GEditor == nullptr)
		{
			return nullptr;
		}

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (ARoadActor* Road = Cast<ARoadActor>(*It))
			{
				return Road;
			}
		}

		return nullptr;
	}

	/**
	 * The road network of the level, whichever editor mode is driving the edit.
	 *
	 * Resolved per call rather than cached: the scene actor belongs to the level, and the modes that own
	 * one today (FEdModeRoad, URoadToolsMode) both create it on entry instead of registering it anywhere
	 * these settings could read.
	 */
	ARoadScene* GetEditingScene()
	{
		if (FEdModeRoad* LegacyMode = FEdModeRoad::Get())
		{
			return LegacyMode->Scene;
		}

		if (GEditor == nullptr)
		{
			return nullptr;
		}

		UWorld* World = GEditor->GetEditorWorldContext().World();
		return (World != nullptr) ? Cast<ARoadScene>(UGameplayStatics::GetActorOfClass(World, ARoadScene::StaticClass())) : nullptr;
	}
}

void USettings_File::Xodr()
{
	if (ARoadScene* Scene = GetEditingScene())
	{
		Scene->ExportXodr();
	}
}

void USettings_RoadPlan::Apply()
{
	ARoadActor* Road = GetEditingRoad();
	if (Road == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(FText::FromName(TEXT("RoadPlan")));
	USettings_RoadPlan* Data = GetMutableDefault<USettings_RoadPlan>();
	Road->Modify();
	Road->ClearLanes();
	Road->InitWithStyle(Data->Style.LoadSynchronous());
	Road->UpdateCurve();
	if (ARoadScene* Scene = Road->GetScene())
	{
		Scene->Rebuild();
	}
}

void USettings_RoadPlan::Create()
{
	if (ARoadActor* Road = GetEditingRoad())
	{
		Road->CreateStyle();
	}
}
#endif