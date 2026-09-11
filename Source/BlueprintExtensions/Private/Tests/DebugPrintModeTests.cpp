/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Libraries/DebugPrintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPrintModeFormatting,
    "XTools.BlueprintExtensions.DebugPrint.ModeFormatting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebugPrintModeFormatting::RunTest(const FString& Parameters)
{
    const TArray<FString> Values{TEXT("A"), TEXT("B")};
    const TArray<FString> Labels{TEXT("Short"), TEXT("Long")};
    TestEqual(TEXT("Inline joins raw values"), UDebugPrintLibrary::FormatDebugValues(Values, Labels, TEXT(" | "), EXToolsDebugPrintMode::Inline), TEXT("A | B"));
    TestEqual(TEXT("Replace joins raw values"), UDebugPrintLibrary::FormatDebugValues(Values, Labels, TEXT(","), EXToolsDebugPrintMode::Replace), TEXT("A,B"));
    TestEqual(TEXT("NewLine omits labels"), UDebugPrintLibrary::FormatDebugValues(Values, Labels, TEXT(" | "), EXToolsDebugPrintMode::NewLine), TEXT("A\nB"));
    TestEqual(TEXT("Labels uses separator"), UDebugPrintLibrary::FormatDebugValues(Values, Labels, TEXT(" = "), EXToolsDebugPrintMode::Labels), TEXT("Short = A\nLong = B"));
    TestEqual(TEXT("Columns aligns labels"), UDebugPrintLibrary::FormatDebugValues(Values, Labels, TEXT(" | "), EXToolsDebugPrintMode::Columns), TEXT("Short | A\nLong  | B"));
    TestEqual(TEXT("Missing labels fall back to index"), UDebugPrintLibrary::FormatDebugValues({TEXT("A"), TEXT("B")}, {}, TEXT(":"), EXToolsDebugPrintMode::Labels), TEXT("1:A\n2:B"));
    TestEqual(TEXT("Empty values are safe"), UDebugPrintLibrary::FormatDebugValues({}, {}, TEXT(":"), EXToolsDebugPrintMode::Columns), FString());
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPrintModeScreenMessages,
    "XTools.BlueprintExtensions.DebugPrint.ModeScreenMessages",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebugPrintModeScreenMessages::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine exists"), GEngine)) { return false; }
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DebugPrintModeTestWorld"));
    if (!TestNotNull(TEXT("Create test world"), World)) { return false; }
    const bool bPreviousScreenMessages = GAreScreenMessagesEnabled;
    const bool bPreviousEngineMessages = GEngine->bEnableOnScreenDebugMessages;
    GAreScreenMessagesEnabled = true;
    GEngine->bEnableOnScreenDebugMessages = true;
    const FLinearColor Color = FLinearColor::White;
    const TArray<FString> Values{TEXT("one"), TEXT("two"), TEXT("three")};
    const TArray<FString> Labels{TEXT("A"), TEXT("B"), TEXT("C")};
    const FString Prefix = TEXT("XToolsDebugPrintTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FName NodeKey(*(Prefix + TEXT("_Node")));
    const FName ExplicitKey(*(Prefix + TEXT("_Explicit")));
    const uint64 NodeHash = GetTypeHash(NodeKey);
    const uint64 ExplicitHash = GetTypeHash(ExplicitKey);
    UDebugPrintLibrary::PrintDebugValues(World, {TEXT("first")}, {}, TEXT(" | "), true, false, Color, 30.f, NAME_None, EXToolsDebugPrintMode::Replace, NodeKey);
    TestTrue(TEXT("Replace uses the generated node key"), GEngine->OnScreenDebugMessageExists(NodeHash));
    UDebugPrintLibrary::PrintDebugValues(World, {TEXT("second")}, {}, TEXT(" | "), true, false, Color, 30.f, NAME_None, EXToolsDebugPrintMode::Replace, NodeKey);
    TestTrue(TEXT("Repeated calls keep the same key"), GEngine->OnScreenDebugMessageExists(NodeHash));
    GEngine->RemoveOnScreenDebugMessage(NodeHash);
    UDebugPrintLibrary::PrintDebugValues(World, {TEXT("explicit")}, {}, TEXT(" | "), true, false, Color, 30.f, ExplicitKey, EXToolsDebugPrintMode::Replace, NodeKey);
    TestTrue(TEXT("Explicit key takes priority"), GEngine->OnScreenDebugMessageExists(ExplicitHash));
    TestFalse(TEXT("Explicit key does not create a generated key"), GEngine->OnScreenDebugMessageExists(NodeHash));
    GEngine->RemoveOnScreenDebugMessage(ExplicitHash);
    for (const EXToolsDebugPrintMode Mode : {EXToolsDebugPrintMode::NewLine, EXToolsDebugPrintMode::Labels, EXToolsDebugPrintMode::Columns})
    {
        UDebugPrintLibrary::PrintDebugValues(World, Values, Labels, TEXT("="), true, false, Color, 30.f, NAME_None, Mode, NodeKey);
        UDebugPrintLibrary::PrintDebugValues(World, Values, Labels, TEXT("="), true, false, Color, 30.f, NAME_None, Mode, NodeKey);
        for (int32 Index = 0; Index < Values.Num(); ++Index)
        {
            const FName LineKey(*(NodeKey.ToString() + TEXT("_") + FString::FromInt(Index)));
            TestTrue(TEXT("Line modes use an indexed stable key"), GEngine->OnScreenDebugMessageExists(GetTypeHash(LineKey)));
            GEngine->RemoveOnScreenDebugMessage(GetTypeHash(LineKey));
        }
    }
    UDebugPrintLibrary::PrintDebugValues(World, {TEXT("inline")}, {}, TEXT(" | "), true, false, Color, 30.f, ExplicitKey, EXToolsDebugPrintMode::Inline, NodeKey);
    TestTrue(TEXT("Inline honors an explicitly supplied key"), GEngine->OnScreenDebugMessageExists(ExplicitHash));
    TestFalse(TEXT("Inline does not use the node key"), GEngine->OnScreenDebugMessageExists(NodeHash));
    GEngine->RemoveOnScreenDebugMessage(ExplicitHash);
    GAreScreenMessagesEnabled = bPreviousScreenMessages;
    GEngine->bEnableOnScreenDebugMessages = bPreviousEngineMessages;
    World->DestroyWorld(false);
    return !HasAnyErrors();
}

#endif
