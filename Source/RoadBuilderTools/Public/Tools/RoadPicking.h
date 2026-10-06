// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/Ray.h"

class ARoadActor;
class ARoadScene;
class URoadBoundary;
class UObject;
struct FPolyline;

/**
 * Picking, standing in for the hit proxies the legacy tools relied on.
 *
 * The legacy layer walked a different element type past the hit-proxy buffer for every tool - HRoadProxy
 * for roads and road points, HRoadCurveProxy for boundaries, HGroundProxy, HJunctionProxy,
 * HRoadMarkingProxy - and let the editor tell it which one a click had landed on. ITF has no hit proxies,
 * so a migrated tool raycasts the same geometry itself. This file is the shared half of that: the
 * geometric primitive, the accumulator, and the element queries more than one tool needs.
 *
 * Screen-size independence: a hit proxy was drawn at a constant screen size, so what counted as "on" the
 * line depended on how far away it was. Scaling a world-space tolerance by the element's distance from the
 * ray origin reproduces that, and keeps an element pickable both up close and from across the level.
 *
 * One deliberate difference: hit proxies were resolved per pixel, so when two elements overlapped the one
 * drawn last won. A ray resolves by distance instead, which is better defined and does not depend on the
 * order a tool happened to draw in. Where a tool draws one element type per pass (which is all of them)
 * the two policies agree.
 */
namespace RoadPicking
{
	/** How close the ray must pass, as a fraction of the element's distance from the ray origin. */
	inline constexpr double DefaultToleranceRatio = 0.02;

	/** Long enough to leave any level. */
	inline constexpr double RayLength = 99999999.0;

	/** Distance from the ray to the segment [Start,End], and the point on the segment closest to it. */
	ROADBUILDERTOOLS_API double SegmentDistance(const FRay& Ray, const FVector& Start, const FVector& End,
		FVector& OutClosest);
}

/**
 * Result of ray-testing one element.
 *
 * The identity fields mirror what the old hit proxies carried, and they mean the same thing here: Owner is
 * the object the element belongs to (a boundary, a marking, a ground, a junction), Index is the
 * sub-element within it, and SubIndex is the second half of an element addressed by a pair - a marking
 * curve's control point and which of its three handles, or a junction's gate and link. A caller that does
 * not need them leaves them at their defaults.
 */
struct FRoadRayHit
{
	/** True when the ray passed within tolerance of the element. False means "considered but too far". */
	bool bHit = false;

	/** Distance from the ray to the element. Smaller means closer to the line of fire. */
	double Distance = 0.0;

	/** Closest point on the element. */
	FVector Position = FVector::ZeroVector;

	/** Object the element belongs to, as labelled by the caller. */
	UObject* Owner = nullptr;

	/** Sub-element (segment / point index), or INDEX_NONE when not applicable. */
	int32 Index = INDEX_NONE;

	/** Second sub-element index, for elements addressed by a pair. */
	int32 SubIndex = INDEX_NONE;
};

/**
 * Accumulates the closest element a ray passes near, carrying whatever identity the caller gave it.
 *
 * A tool considers the elements it happens to be drawing, one at a time, then resolves once - which is how
 * a per-element tolerance can still be applied relative to the element's own distance from the view. The
 * legacy tools did the same thing implicitly, by drawing each element with its own hit proxy.
 *
 * Points outrank segments. A control point sits *on* the curve it belongs to, so the segment through it is
 * never farther from the ray than the point itself - resolving on distance alone would therefore let the
 * curve win every time a point was aimed at, and the point could never be selected. Hit proxies did not
 * have that problem: the point proxy was drawn last, so it won the pixel. The point half is tracked
 * separately here and preferred whenever it passes the tolerance, which restores the legacy precedence.
 */
class ROADBUILDERTOOLS_API FRoadHitCollector
{
public:
	explicit FRoadHitCollector(const FRay& InRay) : Ray(InRay) {}

	/** Considers the segment [Start,End]. */
	void ConsiderSegment(const FVector& Start, const FVector& End, UObject* Owner = nullptr,
		int32 Index = INDEX_NONE, int32 SubIndex = INDEX_NONE);

	/** Considers a single point. Outranks any segment resolved by the same collector. */
	void ConsiderPoint(const FVector& Point, UObject* Owner = nullptr,
		int32 Index = INDEX_NONE, int32 SubIndex = INDEX_NONE);

	/** Considers every segment of a polyline. */
	void ConsiderPolyline(const FPolyline& Curve, UObject* Owner = nullptr,
		int32 Index = INDEX_NONE, int32 SubIndex = INDEX_NONE);

	/** Resolves against the tolerance, or reports a miss when nothing was ever considered. */
	FRoadRayHit Resolve(double ToleranceRatio = RoadPicking::DefaultToleranceRatio) const;

private:
	FRay Ray;
	bool bAnyConsidered = false;
	double BestDistance = TNumericLimits<double>::Max();
	FRoadRayHit Best;

	/** Best point considered so far, kept apart so points can be preferred over segments. */
	bool bAnyPointConsidered = false;
	double BestPointDistance = TNumericLimits<double>::Max();
	FRoadRayHit BestPoint;
};

namespace RoadPicking
{
	/**
	 * Closest road of the scene whose centreline the ray passes near, or null.
	 *
	 * Mirrors what picking an HRoadProxy used to select. A road drawn as a single point has no segment to
	 * walk, so it is tested as the point it is - the same special case the legacy drawing code had.
	 */
	ROADBUILDERTOOLS_API ARoadActor* PickRoad(const ARoadScene* Scene, const FRay& Ray,
		double ToleranceRatio = DefaultToleranceRatio);

	/**
	 * Closest boundary of a road the ray passes near, or null.
	 *
	 * Mirrors picking an HRoadCurveProxy. Only the road's own boundaries are considered, because that is
	 * the set a tool has drawn whenever this is useful.
	 */
	ROADBUILDERTOOLS_API URoadBoundary* PickBoundary(const ARoadActor* Road, const FRay& Ray,
		double ToleranceRatio = DefaultToleranceRatio);

	/**
	 * Closest of a caller-supplied set of boundaries, or null; OutIndex is the winner's position in the
	 * array.
	 *
	 * The narrowed variant exists because some tools offer the ray only part of a road's boundaries: lane
	 * edit lets the right button act on the current lane's two boundaries and nothing else, which is
	 * exactly the set its Render() draws with hit proxies.
	 */
	ROADBUILDERTOOLS_API URoadBoundary* PickBoundary(const TArray<URoadBoundary*>& Boundaries, const FRay& Ray,
		int32& OutIndex, double ToleranceRatio = DefaultToleranceRatio);
}
