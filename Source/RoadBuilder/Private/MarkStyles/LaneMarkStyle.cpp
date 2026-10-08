// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "MarkStyles/LaneMarkStyle.h"
#include "RoadScene.h"

void ULaneMarkStyle::BuildMesh(UObject* Caller, FRoadMesh& Builder, const FPolyline& Curve)
{
	TArray<double> DashOffsets, SolidOffsets;
	AJunctionActor* Junction = Cast<AJunctionActor>(Cast<ARoadActor>(Caller)->GetAttachParentActor());
	switch (MarkType)
	{
	case ELaneMarkType::Dash:
		DashOffsets.Add(0);
		break;
	case ELaneMarkType::Solid:
		SolidOffsets.Add(0);
		break;
	case ELaneMarkType::DashDash:
		DashOffsets.Add(-Separation);
		DashOffsets.Add(+Separation);
		break;
	case ELaneMarkType::DashSolid:
		DashOffsets.Add(-Separation);
		SolidOffsets.Add(+Separation);
		break;
	case ELaneMarkType::SolidDash:
		SolidOffsets.Add(-Separation);
		DashOffsets.Add(+Separation);
		break;
	case ELaneMarkType::SolidSolid:
		SolidOffsets.Add(-Separation);
		SolidOffsets.Add(+Separation);
		break;
	}
	if (DashOffsets.Num())
	{
		double Start = Curve.Points[0].Dist;
		double End = Curve.Points.Last().Dist;
		double Length = End - Start;
		int NumSegments = FMath::RoundToInt(Length / (DashLength + DashSpacing));
		double Step = Length / NumSegments;
		double ActualSpacing = Step - DashLength;
		for (int i = 0; i < NumSegments; i++)
		{
			double Base = Start + Step * i;
			FPolyline SubCurve = Curve.SubCurve(Base + ActualSpacing / 2, Base + ActualSpacing / 2 + DashLength);
			for (double Offset : DashOffsets)
			{
				FPolyline LeftCurve = SubCurve.Offset(FVector2D(Offset - Width / 2, 0), true);
				FPolyline RightCurve = SubCurve.Offset(FVector2D(Offset + Width / 2, 0), true);
				if (Junction)
				{
					Junction->FixHeight(LeftCurve);
					Junction->FixHeight(RightCurve);
				}
				Builder.AddStrip(Material, LeftCurve, RightCurve);
			}
		}
	}
	for (double Offset : SolidOffsets)
	{
		FPolyline LeftCurve = Curve.Offset(FVector2D(Offset - Width / 2, 0), true);
		FPolyline RightCurve = Curve.Offset(FVector2D(Offset + Width / 2, 0), true);
		if (Junction)
		{
			Junction->FixHeight(LeftCurve);
			Junction->FixHeight(RightCurve);
		}
		check(!LeftCurve.ContainsNaN());
		check(!RightCurve.ContainsNaN());
		Builder.AddStrip(Material, LeftCurve, RightCurve);
	}
}
