// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/RoadInteractiveTool.h"

#include "RoadTool_RoadSplit.generated.h"

/** Builder for URoadTool_RoadSplit. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadSplitBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Splits the selected road along one of its boundaries.
 *
 * Right click on a boundary of the selected road splits it there; left click selects a road. While a road
 * is selected the tool draws its boundaries instead of the roads, so the target of the forthcoming right
 * click is what is on screen - which is the only thing the legacy hit-proxy pass was really expressing,
 * and which a ray test reproduces without needing proxies.
 *
 * | legacy (FModeTool)                            | here                                       |
 * |-----------------------------------------------|--------------------------------------------|
 * | HandleClickRoad(HRoadProxy)                    | SelectRoadUnderRay()                        |
 * | HitProxyCast<HRoadCurveProxy>                  | RoadPicking::PickBoundary()                 |
 * | FScopedTransaction + GetScene()->Rebuild()     | FRoadUndoTransaction + RequestRebuild()     |
 * | Render(PDI) + SetHitProxy(HRoadCurveProxy)     | Render(RenderAPI), no hit proxies           |
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_RoadSplit : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;
};
