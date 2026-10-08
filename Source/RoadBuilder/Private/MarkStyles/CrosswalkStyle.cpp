// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "MarkStyles/CrosswalkStyle.h"
#include "RoadScene.h"

void UCrosswalkStyle::BuildMesh(UObject* Caller, FRoadMesh& Builder, const FPolyline& Curve)
{
	FPolyline Points = Curve.Resample(DashLength + DashGap);
	AJunctionActor* Junction = Cast<AJunctionActor>(Cast<ARoadActor>(Caller)->GetAttachParentActor());
	for (int i = 0; i < Points.Points.Num(); i++)
	{
		FVector VDir = Points.GetStraightDir(i);
		FVector HDir(-VDir.Y, VDir.X, VDir.Z);
		FVector& Pos = Points.Points[i].Pos;
		FPolyline LeftCurve({ FPolyPoint(Pos + VDir * DashLength / 2 - HDir * Width / 2, 0), FPolyPoint(Pos + VDir * DashLength / 2 + HDir * Width / 2, Width) });
		FPolyline RightCurve({ FPolyPoint(Pos - VDir * DashLength / 2 - HDir * Width / 2, 0), FPolyPoint(Pos - VDir * DashLength / 2 + HDir * Width / 2, Width) });
		if (Junction)
		{
			Junction->FixHeight(LeftCurve);
			Junction->FixHeight(RightCurve);
		}
		Builder.AddStrip(Material, LeftCurve, RightCurve);
	}
}
