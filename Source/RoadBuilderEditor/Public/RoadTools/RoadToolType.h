#pragma once
#include "CoreMinimal.h"

/**
 * Tool identities.
 *
 * Every FRoadTool reports its own value via GetToolType(); FEdModeRoad uses it as the TMap key to look up tools,
 * so **the enum values themselves carry no meaning**: adding, reordering or inserting tools never affects other tools,
 * and there is no implicit rule like "enum order must match Tools.Add order".
 *
 * It lives in its own header (instead of RoadTool.h) so consumers that only care about tool identity (e.g. FRoadToolbar)
 * depend only on this tiny header without dragging in Slate / PropertyEditor / RoadScene.
 */
enum class ERoadToolType : uint8
{
	/** No current tool. */
	None = 0,
	File,
	RoadPlan,
	RoadHeight,
	RoadChop,
	RoadSplit,
	JunctionLink,
	LaneEdit,
	LaneCarve,
	LaneWidth,
	MarkingLane,
	MarkingPoint,
	MarkingCurve,
	GroundEdit,
	Settings,
};
