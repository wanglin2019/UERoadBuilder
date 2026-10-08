// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MarkStyles/BaseMarkStyle.h"
#include "CrosswalkStyle.generated.h"

UCLASS()
class ROADBUILDER_API UCrosswalkStyle : public UBaseMarkStyle
{
	GENERATED_BODY()
public:
	virtual void BuildMesh(UObject* Caller, FRoadMesh& Builder, const FPolyline& Curve);

	UPROPERTY(EditAnywhere, Category = Style)
	double Width = 250;

	UPROPERTY(EditAnywhere, Category = Style)
	double BorderWidth = 0;

	UPROPERTY(EditAnywhere, Category = Style)
	double DashLength = 50.f;

	UPROPERTY(EditAnywhere, Category = Style)
	double DashGap = 50.f;
};
