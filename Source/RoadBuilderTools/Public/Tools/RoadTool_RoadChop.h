// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/RoadInteractiveTool.h"

#include "RoadTool_RoadChop.generated.h"

/** Builder for URoadTool_RoadChop. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadChopBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Chops the selected road in two, or joins another road onto it.
 *
 * The legacy gesture is preserved: left click selects a road, right click acts on the road under the
 * cursor - chop when that is the selected road, join when it is a different one.
 *
 * | legacy (FModeTool)                                   | here                                             |
 * |------------------------------------------------------|--------------------------------------------------|
 * | HandleClickRoad(HRoadProxy)                          | SelectRoadUnderRay()                             |
 * | Click.GetKey() == EKeys::RightMouseButton            | a second click behaviour bound to the right button |
 * | HitProxyCast<HRoadProxy> for the action target       | RoadPicking::PickRoad()                          |
 * | LineTrace(InViewportClient)                          | LineTrace(ray)                                   |
 * | FScopedTransaction + GetScene()->Rebuild()           | FRoadUndoTransaction + RequestRebuild()          |
 * | Render(PDI) + hit proxies                            | Render(RenderAPI), no hit proxies                |
 *
 * NotifyPreChange() has no counterpart: it existed so that details-panel edits made through the mode
 * could call Modify() on the road. RoadChop registers no property set, so there is no such path.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadChop : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;
};
