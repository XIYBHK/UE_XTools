#include "BlueprintAssistMisc/IBAInputState.h"

#include "BlueprintAssistInputProcessor.h"

IBAInputState& IBAInputState::Get()
{
	return FBAInputProcessor::Get();
}
