#pragma once

#include "AIController.h"
#include "SplineMoveAlongActionTestTypes.generated.h"

UCLASS()
class ASplineMovementTestController : public AAIController
{
	GENERATED_BODY()

public:
	int32 StopMovementCalls = 0;

	virtual void StopMovement() override
	{
		++StopMovementCalls;
		AAIController::StopMovement();
	}
};
