// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Grabbable.generated.h"

UINTERFACE(MinimalAPI)
class UGrabbable : public UInterface
{
	GENERATED_BODY()
};

class GRAVITYGUN_API IGrabbable
{
	GENERATED_BODY()

public:
	virtual void OnGrab();
	virtual void OnRelease();
};
