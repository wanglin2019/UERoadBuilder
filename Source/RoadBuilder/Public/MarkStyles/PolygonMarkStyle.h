// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MarkStyles/BaseMarkStyle.h"
#include "PolygonMarkStyle.generated.h"

UENUM()
enum class EPolygonMarkType : uint8
{
	Solid,
	Striped,
	Crosshatch,
	Chevron,
};

UCLASS()
class ROADBUILDER_API UPolygonMarkStyle : public UBaseMarkStyle
{
	GENERATED_BODY()
public:
	void BuildMesh(UObject* Caller, FRoadMesh& Builder, const FPolyline& Curve);

	UPROPERTY(EditAnywhere, Category = Style)
	EPolygonMarkType MarkType;

	UPROPERTY(EditAnywhere, Category = Style)
	double LineSpace = 200;

	UPROPERTY(EditAnywhere, Category = Style, meta = (ClampMin = 30, ClampMax = 60))
	double ChevrenAngle = 45;
};
