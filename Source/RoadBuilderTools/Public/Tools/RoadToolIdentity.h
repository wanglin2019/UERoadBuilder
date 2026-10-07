// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Tool identities, as consumed by UEdMode::RegisterTool() and
 * UInteractiveToolManager::RegisterToolType().
 *
 * ITF identifies a tool by a string, not by a C++ type, which conveniently removes the failure mode the
 * legacy layer had to engineer around: there, tool identity was the array index into FModeTool::Tools,
 * so inserting a tool silently shifted every tool at 14 call sites. Here the identity is written down
 * once and referenced by the tool class, the command registration and the host.
 *
 * This header is now the single identity list for the new tool layer. The legacy ERoadToolType enum is
 * not being retired - the legacy module stays as the reference implementation - so the two lists are
 * kept in step by hand: every entry here names a tool that the legacy module also has, and adding a
 * tool means adding it on both sides.
 */
namespace RoadToolIds
{
	/** Places road alignment points. S0 pilot tool: proves host registration, input, render and context injection. */
	inline const TCHAR* RoadPlan = TEXT("RoadPlan");

	/**
	 * Edits the height profile of the selected road: pick a height point, drag it along the road,
	 * edit its numbers, delete it.
	 *
	 * S0.5 pilot tool. It is deliberately the hardest of the legacy tools to port, because it is the
	 * only one that exercises the whole risk surface at once:
	 *   - the transform gizmo protocol (legacy GetWidgetLocation / GetCustomDrawingCoordinateSystem /
	 *     InputDelta) which is the seam most likely to break when the editor's gizmo system changes
	 *   - picking without hit proxies (ITF has no HHitProxy), so the tool raycasts against road data
	 *   - keyboard input (legacy InputKey), for the Delete binding
	 *   - the details panel editing live road data, with undo
	 */
	inline const TCHAR* RoadHeight = TEXT("RoadHeight");

	/** Exports the road network. Panel-only: shows USettings_File and does nothing else. */
	inline const TCHAR* File = TEXT("File");

	/** Global settings. Panel-only: shows USettings_Global and does nothing else. */
	inline const TCHAR* Settings = TEXT("Settings");

	/** Chops the selected road at a point, or joins another road onto it. */
	inline const TCHAR* RoadChop = TEXT("RoadChop");

	/** Splits the selected road along one of its boundaries. */
	inline const TCHAR* RoadSplit = TEXT("RoadSplit");

	/** Carves a lane transition between two points on a road's boundary. */
	inline const TCHAR* LaneCarve = TEXT("LaneCarve");

	/** Picks a lane segment on a road and moves it, or copies a lane across a boundary. */
	inline const TCHAR* LaneEdit = TEXT("LaneEdit");

	/** Widens or narrows a road by editing the width control points along one of its boundaries. */
	inline const TCHAR* LaneWidth = TEXT("LaneWidth");

	/** Edits the lane markings carried by a road's boundary segments. */
	inline const TCHAR* MarkingLane = TEXT("MarkingLane");

	/** Places and edits point markings - manhole covers, bollards - on a road. */
	inline const TCHAR* MarkingPoint = TEXT("MarkingPoint");

	/** Draws and edits spline markings - lane lines, gore areas, hatched boxes - on a road. */
	inline const TCHAR* MarkingCurve = TEXT("MarkingCurve");

	/** Edits the outline a ground area is built from. */
	inline const TCHAR* GroundEdit = TEXT("GroundEdit");

	/** Inspects and tunes the link curves a junction generates between the roads meeting there. */
	inline const TCHAR* JunctionLink = TEXT("JunctionLink");
}
