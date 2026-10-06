// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_RoadChop.h"

#include "RoadActor.h"
#include "SceneManagement.h"
#include "Tools/RoadPicking.h"

#define LOCTEXT_NAMESPACE "RoadTool_RoadChop"

UInteractiveTool* URoadTool_RoadChopBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_RoadChop* NewTool = NewObject<URoadTool_RoadChop>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_RoadChop::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();
}

void URoadTool_RoadChop::Render(IToolsContextRenderAPI* RenderAPI)
{
	UInteractiveTool::Render(RenderAPI);

	FPrimitiveDrawInterface* PDI = (RenderAPI != nullptr) ? RenderAPI->GetPrimitiveDrawInterface() : nullptr;
	if (PDI != nullptr)
	{
		DrawRoads(PDI);
	}
}

void URoadTool_RoadChop::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;

	if (!bRightButton)
	{
		// Left click selects the road under the cursor, which is all the legacy HandleClickRoad() did.
		SelectRoadUnderRay(Ray);
		return;
	}

	// Right click acts, and needs both a selected road to act on and a road under the cursor to act with.
	// Nothing picked means nothing happens, rather than a selection change - the legacy tool did the same.
	ARoadActor* SelectedRoad = GetSelectedRoad();
	if (SelectedRoad == nullptr)
	{
		return;
	}

	ARoadActor* CursorRoad = RoadPicking::PickRoad(GetRoadScene(), Ray);
	if (CursorRoad == nullptr)
	{
		return;
	}

	// The chop position is the cursor projected onto the road, so it needs a real world hit. A miss leaves
	// the sentinel, and projecting the sentinel would chop at an arbitrary distance - so it is dropped.
	const FVector Position = LineTrace(Ray);
	if (Position.X >= WORLD_MAX)
	{
		return;
	}
	const double Dist = SelectedRoad->GetUV(Position).X;

	// Chop() spawns a second road actor and Join() destroys one, so this is not a change to a single
	// object and EmitObjectChange() cannot express it. A host transaction can, and it is what the legacy
	// FScopedTransaction was for.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("RoadChop", "Road Chop"));
		if (CursorRoad == SelectedRoad)
		{
			SelectedRoad->Chop(Dist);
		}
		else
		{
			SelectedRoad->Join(CursorRoad);
		}
	}

	RequestRebuild();
}

#undef LOCTEXT_NAMESPACE
