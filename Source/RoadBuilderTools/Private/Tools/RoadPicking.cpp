// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "Tools/RoadPicking.h"

#include "RoadActor.h"
#include "RoadCurve.h"
#include "RoadScene.h"

namespace RoadPicking
{
	double SegmentDistance(const FRay& Ray, const FVector& Start, const FVector& End, FVector& OutClosest)
	{
		const FVector RayEnd = Ray.Origin + Ray.Direction * RayLength;
		FVector RayPoint;
		FMath::SegmentDistToSegmentSafe(Ray.Origin, RayEnd, Start, End, RayPoint, OutClosest);
		return FVector::Dist(RayPoint, OutClosest);
	}
}

void FRoadHitCollector::ConsiderSegment(const FVector& Start, const FVector& End, UObject* Owner,
	int32 Index, int32 SubIndex)
{
	FVector Closest;
	const double Distance = RoadPicking::SegmentDistance(Ray, Start, End, Closest);
	bAnyConsidered = true;
	if (Distance < BestDistance)
	{
		BestDistance = Distance;
		Best.Position = Closest;
		Best.Owner = Owner;
		Best.Index = Index;
		Best.SubIndex = SubIndex;
	}
}

void FRoadHitCollector::ConsiderPoint(const FVector& Point, UObject* Owner, int32 Index, int32 SubIndex)
{
	// A point is a zero-length segment, so it goes through the same distance test a curve does. That keeps
	// the "distance from the view scales the tolerance" rule identical for points and lines.
	ConsiderSegment(Point, Point, Owner, Index, SubIndex);

	// ...but it is also measured and recorded on its own record, because a point outranks the curve it sits
	// on and the general "closest wins" accumulator above cannot express that. The distance is computed
	// here rather than read back off Best: by the time this runs, Best may already hold a nearer segment,
	// and borrowing it would file a curve under the point entry. See the class comment.
	FVector Closest;
	const double PointDistance = RoadPicking::SegmentDistance(Ray, Point, Point, Closest);
	bAnyPointConsidered = true;
	if (PointDistance < BestPointDistance)
	{
		BestPointDistance = PointDistance;
		BestPoint.Position = Closest;
		BestPoint.Owner = Owner;
		BestPoint.Index = Index;
		BestPoint.SubIndex = SubIndex;
	}
}

void FRoadHitCollector::ConsiderPolyline(const FPolyline& Curve, UObject* Owner, int32 Index, int32 SubIndex)
{
	for (int32 PointIndex = 0; PointIndex < Curve.Points.Num() - 1; ++PointIndex)
	{
		ConsiderSegment(Curve.Points[PointIndex].Pos, Curve.Points[PointIndex + 1].Pos, Owner, Index, SubIndex);
	}
}

FRoadRayHit FRoadHitCollector::Resolve(double ToleranceRatio) const
{
	FRoadRayHit Hit;
	if (!bAnyConsidered)
	{
		// Nothing was ever considered: report a miss rather than a hit at the origin.
		return Hit;
	}

	// A point that is within tolerance wins outright. Aiming at a control point means aiming at the curve
	// it sits on at the same time, so without this preference the curve would always take the pick and no
	// control point could ever be selected.
	if (bAnyPointConsidered
		&& BestPointDistance <= FVector::Dist(Ray.Origin, BestPoint.Position) * ToleranceRatio)
	{
		Hit = BestPoint;
		Hit.Distance = BestPointDistance;
		Hit.bHit = true;
		return Hit;
	}

	Hit = Best;
	Hit.Distance = BestDistance;
	Hit.bHit = (BestDistance <= FVector::Dist(Ray.Origin, Best.Position) * ToleranceRatio);
	return Hit;
}

ARoadActor* RoadPicking::PickRoad(const ARoadScene* Scene, const FRay& Ray, double ToleranceRatio)
{
	if (Scene == nullptr)
	{
		return nullptr;
	}

	ARoadActor* BestRoad = nullptr;
	FRoadRayHit BestHit;
	for (ARoadActor* Road : Scene->Roads)
	{
		if (Road == nullptr || Road->BaseCurve == nullptr)
		{
			continue;
		}

		FRoadHitCollector Collector(Ray);
		Collector.ConsiderPolyline(Road->BaseCurve->Curve);
		if (Road->BaseCurve->Curve.Points.Num() == 1)
		{
			Collector.ConsiderPoint(Road->BaseCurve->Curve.Points[0].Pos);
		}

		const FRoadRayHit Hit = Collector.Resolve(ToleranceRatio);
		if (Hit.bHit && (!BestHit.bHit || Hit.Distance < BestHit.Distance))
		{
			BestHit = Hit;
			BestRoad = Road;
		}
	}

	return BestRoad;
}

URoadBoundary* RoadPicking::PickBoundary(const ARoadActor* Road, const FRay& Ray, double ToleranceRatio)
{
	if (Road == nullptr)
	{
		return nullptr;
	}

	URoadBoundary* BestBoundary = nullptr;
	FRoadRayHit BestHit;
	for (URoadBoundary* Boundary : Road->Boundaries)
	{
		if (Boundary == nullptr)
		{
			continue;
		}

		FRoadHitCollector Collector(Ray);
		Collector.ConsiderPolyline(Boundary->Curve);

		const FRoadRayHit Hit = Collector.Resolve(ToleranceRatio);
		if (Hit.bHit && (!BestHit.bHit || Hit.Distance < BestHit.Distance))
		{
			BestHit = Hit;
			BestBoundary = Boundary;
		}
	}

	return BestBoundary;
}

URoadBoundary* RoadPicking::PickBoundary(const TArray<URoadBoundary*>& Boundaries, const FRay& Ray,
	int32& OutIndex, double ToleranceRatio)
{
	URoadBoundary* BestBoundary = nullptr;
	FRoadRayHit BestHit;
	OutIndex = INDEX_NONE;
	for (int32 BoundaryIndex = 0; BoundaryIndex < Boundaries.Num(); ++BoundaryIndex)
	{
		URoadBoundary* Boundary = Boundaries[BoundaryIndex];
		if (Boundary == nullptr)
		{
			continue;
		}

		FRoadHitCollector Collector(Ray);
		Collector.ConsiderPolyline(Boundary->Curve);

		const FRoadRayHit Hit = Collector.Resolve(ToleranceRatio);
		if (Hit.bHit && (!BestHit.bHit || Hit.Distance < BestHit.Distance))
		{
			BestHit = Hit;
			BestBoundary = Boundary;
			OutIndex = BoundaryIndex;
		}
	}

	return BestBoundary;
}
