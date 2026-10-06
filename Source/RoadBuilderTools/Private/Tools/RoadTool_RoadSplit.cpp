// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadTool_RoadSplit.h"

#include "RoadActor.h"
#include "RoadBoundary.h"
#include "SceneManagement.h"
#include "Settings.h"
#include "Tools/RoadPicking.h"

#define LOCTEXT_NAMESPACE "RoadTool_RoadSplit"

UInteractiveTool* URoadTool_RoadSplitBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	URoadTool_RoadSplit* NewTool = NewObject<URoadTool_RoadSplit>(SceneState.ToolManager);
	NewTool->SetEditingWorld(SceneState.World);
	return NewTool;
}

void URoadTool_RoadSplit::Setup()
{
	UInteractiveTool::Setup();

	AddClickBehavior();

	// The junction dimensions Split() builds with - junction size, corner radius, shoulder width. They live
	// on the shared USettings_RoadSplit CDO, which ARoadActor::Split() reads, so showing that CDO as a
	// property source is all it takes to make them editable here. The legacy tool reached the same panel
	// through FRoadEdMode's GetToolSettings() mapping; the panel itself is unchanged.
	AddToolPropertySource(GetMutableDefault<USettings_RoadSplit>());
}

void URoadTool_RoadSplit::Render(IToolsContextRenderAPI* RenderAPI)
{
	UInteractiveTool::Render(RenderAPI);

	FPrimitiveDrawInterface* PDI = (RenderAPI != nullptr) ? RenderAPI->GetPrimitiveDrawInterface() : nullptr;
	if (PDI == nullptr)
	{
		return;
	}

	ARoadActor* SelectedRoad = GetSelectedRoad();
	if (SelectedRoad != nullptr && SelectedRoad->Length() > 0.0)
	{
		// A road is selected, so its boundaries are what a right click can act on; show them and nothing
		// else, exactly as the legacy Render() did.
		for (URoadBoundary* Boundary : SelectedRoad->Boundaries)
		{
			if (Boundary != nullptr)
			{
				DrawCurve(PDI, Boundary->Curve, RoadToolStyle::Color_Line, RoadToolStyle::Thickness_Line);
			}
		}
		return;
	}

	// Nothing selected yet, so offer the roads to select from.
	DrawRoads(PDI);
}

void URoadTool_RoadSplit::OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton)
{
	const FRay& Ray = ClickPos.WorldRay;

	if (!bRightButton)
	{
		SelectRoadUnderRay(Ray);
		return;
	}

	ARoadActor* SelectedRoad = GetSelectedRoad();
	if (SelectedRoad == nullptr || SelectedRoad->Length() <= 0.0)
	{
		return;
	}

	URoadBoundary* Boundary = RoadPicking::PickBoundary(SelectedRoad, Ray);
	if (Boundary == nullptr)
	{
		return;
	}

	// Split() spawns a new road actor, so the edit is bracketed by a host transaction - the same reason
	// the legacy tool used FScopedTransaction rather than Modify() plus an inspector round trip.
	{
		FRoadUndoTransaction Transaction(GetToolManager(), LOCTEXT("RoadSplit", "Road Split"));
		SelectedRoad->Split(Boundary);
	}

	RequestRebuild();
}

#undef LOCTEXT_NAMESPACE
