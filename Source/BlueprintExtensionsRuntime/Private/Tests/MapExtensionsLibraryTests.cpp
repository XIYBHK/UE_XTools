#include "MapExtensionsLibraryTestTypes.h"

int32 FMapExtensionsTrackedValue::LiveInstanceCount = 0;

FMapExtensionsTrackedValue::FMapExtensionsTrackedValue()
{
	++LiveInstanceCount;
}

FMapExtensionsTrackedValue::FMapExtensionsTrackedValue(const FMapExtensionsTrackedValue& Other)
	: Text(Other.Text)
{
	++LiveInstanceCount;
}

FMapExtensionsTrackedValue::FMapExtensionsTrackedValue(FMapExtensionsTrackedValue&& Other) noexcept
	: Text(MoveTemp(Other.Text))
{
	++LiveInstanceCount;
}

FMapExtensionsTrackedValue::~FMapExtensionsTrackedValue()
{
	--LiveInstanceCount;
}

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Libraries/MapExtensionsLibrary.h"

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMapExtensionsLibrary_ClearsConstructedOutputs,
	"XTools.BlueprintExtensionsRuntime.Map.ClearsConstructedOutputs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapExtensionsLibrary_ClearsConstructedOutputs::RunTest(const FString& Parameters)
{
	FMapExtensionsTestContainer Container;
	const FMapProperty* MapProperty = FindFProperty<FMapProperty>(
		FMapExtensionsTestContainer::StaticStruct(),
		GET_MEMBER_NAME_CHECKED(FMapExtensionsTestContainer, Values));
	if (!TestNotNull(TEXT("Map property must be available"), MapProperty))
	{
		return false;
	}

	FMapExtensionsTrackedValue OutValue;
	OutValue.Text = TEXT("stale");
	int32 OutKey = 42;
	const int32 LiveCountBeforeReset = FMapExtensionsTrackedValue::LiveInstanceCount;

	UMapExtensionsLibrary::GenericMap_RandomItem(
		&Container.Values,
		MapProperty,
		&OutKey,
		&OutValue);

	TestEqual(TEXT("Empty map must clear the output key"), OutKey, 0);
	TestTrue(TEXT("Empty map must clear the output value"), OutValue.Text.IsEmpty());
	TestEqual(
		TEXT("Clearing an initialized struct output must not construct another live value"),
		FMapExtensionsTrackedValue::LiveInstanceCount,
		LiveCountBeforeReset);

	return true;
}

#endif
