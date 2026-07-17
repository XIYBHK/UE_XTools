#pragma once

#include "InputCoreTypes.h"

struct FInputChord;

class IBAInputState
{
public:
	IBAInputState() = default;
	virtual ~IBAInputState() = default;

	static IBAInputState& Get();

	virtual bool IsInputChordDown(const FInputChord& Chord) = 0;
	virtual bool IsAnyInputChordDown(const TArray<FInputChord>& Chords) = 0;
	virtual bool IsInputChordDown(const FInputChord& Chord, const FKey Key) = 0;
	virtual bool IsAnyInputChordDown(const TArray<FInputChord>& Chords, const FKey Key) = 0;
	virtual bool IsKeyDown(const FKey Key) = 0;
	virtual double GetKeyDownDuration(const FKey Key) = 0;
};
