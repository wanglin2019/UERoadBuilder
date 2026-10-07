// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/RoadInteractiveTool.h"

#include "RoadTool_LaneCarve.generated.h"

class URoadLane;

/** Builder for URoadTool_LaneCarve. */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_LaneCarveBuilder : public URoadInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/**
 * Carves a lane transition ("fork") between two points on a road, in two clicks.
 *
 * Legacy gesture, preserved: left click selects a road; the first right click on a boundary arms the
 * carve, the second one performs it. Between the two clicks the tool rubber-bands a line to the cursor so
 * the span is visible before it is paid for.
 *
 * | legacy (FModeTool)                             | here                                        |
 * |------------------------------------------------|---------------------------------------------|
 * | HandleClickRoad(HRoadProxy)                     | SelectRoadUnderRay()                         |
 * | HitProxyCast<HRoadCurveProxy>                   | RoadPicking::PickBoundary()                  |
 * | LineTrace(ViewportClient) for the cursor        | AddHoverBehavior() + LineTrace(cursor ray)   |
 * | FScopedTransaction + per-object Modify()        | FRoadUndoTransaction + kept Modify() calls   |
 * | StartUV member + Reset() clearing it            | StartUV member + ResetCarve()                |
 * | Render(PDI) + hit proxies                       | Render(RenderAPI), no hit proxies            |
 *
 * The Modify() calls survive the migration on purpose. They are not editor API - UObject::Modify() is a
 * runtime entry point that records the object with whatever undo stack the host installed - and they are
 * what makes the lane and boundary sub-objects, not just the road, part of the transaction.
 */
UCLASS()
class ROADBUILDERTOOLS_API URoadTool_LaneCarve : public URoadInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual bool OnRoadClicked(const FInputDeviceRay& ClickPos, bool bRightButton) override;

private:
	/**
	 * Arms the carve at the boundary under the cursor, or performs it when one is already armed.
	 * Returns whether the click was this tool's business at all (a road selected, a boundary under the
	 * cursor and real ground beneath it) - the same condition the legacy HRoadCurveProxy hit expressed.
	 */
	bool CarveAtRay(const FRay& Ray);

	/** Performs the carve from the armed StartUV to EndUV. Returns whether anything changed. */
	bool ApplyCarve(ARoadActor* Road, const FVector2D& EndUV);

	/** Disarms the carve. The legacy LaneCarve::Reset() equivalent, minus the panel it also cleared. */
	void ResetCarve();

	/**
	 * Armed start of the carve as (station, offset).
	 *
	 * X < 0 means nothing is armed, which is the legacy sentinel: a station is never negative, so the sign
	 * doubles as the "armed?" flag without a second member.
	 */
	FVector2D StartUV = FVector2D(-1, 0);
};
