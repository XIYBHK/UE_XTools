#include "Libraries/TransformExtensionsLibrary.h"

#pragma region TransformMember

const FVector& UTransformExtensionsLibrary::TLocation(const FTransform& Transform)
{
	static FVector Location;
	Location = Transform.GetTranslation();
	return Location;
}

const FRotator& UTransformExtensionsLibrary::TRotation(const FTransform& Transform)
{
	static FRotator Rotation;
	Rotation = Transform.Rotator();
	return Rotation;
}

#pragma endregion

#pragma region TransformDirection

FVector UTransformExtensionsLibrary::AxisForward(const FTransform& Transform)
{
	return Transform.GetUnitAxis(EAxis::X);
}

FVector UTransformExtensionsLibrary::AxisRight(const FTransform& Transform)
{
	return Transform.GetUnitAxis(EAxis::Y);
}

FVector UTransformExtensionsLibrary::AxisUp(const FTransform& Transform)
{
	return Transform.GetUnitAxis(EAxis::Z);
}

#pragma endregion
