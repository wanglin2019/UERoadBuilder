// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "RoadCurve.h"
#include "RoadMesh.h"
#include "Materials/MaterialInterface.h"
#include "BaseMarkStyle.generated.h"

UCLASS()
class ROADBUILDER_API UBaseMarkStyle : public UObject
{
	GENERATED_BODY()
public:
	virtual void BuildMesh(UObject* Caller, FRoadMesh& Builder, const FPolyline& Curve) {}
	UPROPERTY(EditAnywhere, Category = Style)
	UMaterialInterface* Material = nullptr;
};
