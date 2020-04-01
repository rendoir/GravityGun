#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grabbable.h"
#include "GrabbableActor.generated.h"

UCLASS()
class GRAVITYGUN_API AGrabbableActor : public AActor, public IGrabbable
{
	GENERATED_BODY()

public:
	virtual void OnGrab() override;
	virtual void OnRelease() override;
};
