// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MarkStyles/BaseMarkStyle.h"
#include "LaneMarkStyle.generated.h"

UENUM()
enum class ELaneMarkType : uint8
{
	Dash,
	Solid,
	DashDash,
	DashSolid,
	SolidDash,
	SolidSolid,
};

UCLASS()
class ROADBUILDER_API ULaneMarkStyle : public UBaseMarkStyle
{
	GENERATED_BODY()
public:
	virtual void BuildMesh(UObject* Caller, FRoadMesh& Builder, const FPolyline& Curve);
	FString GetXodrType()
	{
		if (MarkType == ELaneMarkType::Dash) return TEXT("broken");
		if (MarkType == ELaneMarkType::Solid) return TEXT("solid");
		if (MarkType == ELaneMarkType::DashDash) return TEXT("broken broken");
		if (MarkType == ELaneMarkType::DashSolid) return TEXT("broken solid");
		if (MarkType == ELaneMarkType::SolidDash) return TEXT("solid broken");
		if (MarkType == ELaneMarkType::SolidSolid) return TEXT("solid solid");
		return TEXT("none");
	}
	FString GetXodrColor()
	{
		if (Material)
		{
			FString Name = Material->GetName();
			if (Name.Contains(TEXT("yellow"))) return TEXT("yellow");
			if (Name.Contains(TEXT("white"))) return TEXT("white");
		}
		return TEXT("standard");
	}
	UPROPERTY(EditAnywhere, Category = Style)
	ELaneMarkType MarkType = (ELaneMarkType)0;

	UPROPERTY(EditAnywhere, Category = Style)
	double Width = 12.5f;

	UPROPERTY(EditAnywhere, Category = Style)
	double Separation = 12.5f;

	UPROPERTY(EditAnywhere, Category = Style)
	double DashLength = 150.f;

	UPROPERTY(EditAnywhere, Category = Style)
	double DashSpacing = 150.f;
};
