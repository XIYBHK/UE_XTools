/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "K2Nodes/K2Node_DebugPrint.h"
#include "Libraries/DebugPrintLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_MakeArray.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Script.h"
#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPrintExecution, "XTools.BlueprintExtensions.DebugPrint.Execution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDebugPrintExecution::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("ScanPathsSynchronous: Package /Engine/Transient does not exist"), EAutomationExpectedErrorFlags::Contains, 1);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("DebugPrintTest")), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("RunPrint"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, true, nullptr);
    auto* Entry = FBlueprintEditorUtils::GetEntryNode(Graph);
    auto* Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
    auto* Print = NewObject<UK2Node_DebugPrint>(Graph);
    Graph->AddNode(Print, false, false);
    Print->CreateNewGuid();
    Print->AllocateDefaultPins();
    Print->AddInputPin();
    Print->AddInputPin();
    // 删除中间值后保留 Value_2 的身份，重建后仍打印其默认值。
    Print->FindPin(TEXT("Value_2"))->PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    Print->FindPin(TEXT("Value_2"))->DefaultValue = TEXT("DebugPrint sentinel");
    Print->RemoveInputPin(Print->FindPin(TEXT("Value_1")));
    TestNotNull(TEXT("删除中间引脚应保留后续稳定名称"), Print->FindPin(TEXT("Value_2")));
    TestEqual(TEXT("重建保留字符串默认值"), Print->FindPin(TEXT("Value_2"))->DefaultValue, FString(TEXT("DebugPrint sentinel")));

    auto* Array = NewObject<UK2Node_MakeArray>(Graph);
    Graph->AddNode(Array, false, false);
    Array->NumInputs = 2;
    Array->AllocateDefaultPins();
    Array->GetOutputPin()->PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    for (int32 Index = 0; Index < 2; ++Index)
    {
        auto* Pin = Array->FindPin(Array->GetPinName(Index));
        Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        Pin->DefaultValue = FString::FromInt(7 + Index);
    }
    const UEdGraphSchema* Schema = Graph->GetSchema();
    TestTrue(TEXT("数组可连接通配输入"), Schema->TryCreateConnection(Array->GetOutputPin(), Print->FindPin(TEXT("Value_0"))));
    Print->AddInputPin();
    auto* Vector = Print->FindPin(TEXT("Value_3"));
    Vector->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    Vector->PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    Schema->SplitPin(Vector);
    if (!TestEqual(TEXT("向量应拆分为三个分量"), Vector->SubPins.Num(), 3)) { return false; }
    Vector->SubPins[0]->DefaultValue = TEXT("12.0");
    Vector->SubPins[1]->DefaultValue = TEXT("34.0");
    Vector->SubPins[2]->DefaultValue = TEXT("56.0");
    Print->ReconstructNode();
    Print->FindPin(TEXT("bPrintToScreen"))->DefaultValue = TEXT("false");
    TestTrue(TEXT("连接入口"), Schema->TryCreateConnection(Entry->FindPin(UEdGraphSchema_K2::PN_Then), Print->GetExecPin()));
    TestTrue(TEXT("连接返回"), Schema->TryCreateConnection(Print->GetThenPin(), Result->FindPin(UEdGraphSchema_K2::PN_Execute)));
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TestTrue(TEXT("调试打印蓝图编译成功"), Blueprint->Status != BS_Error);
    if (Blueprint->Status == BS_Error || !Blueprint->GeneratedClass) { return false; }
    UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(TEXT("RunPrint"));
    if (!TestNotNull(TEXT("编译生成函数"), Function)) { return false; }
    struct FPrintCapture : FOutputDevice
    {
        FString Message;
        void Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const FName& Category) override
        {
            if (Category == TEXT("LogBlueprintUserMessages") && FString(Text).Contains(TEXT("DebugPrint sentinel"))) { Message = Text; }
        }
    } Capture;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DebugPrintTestWorld"));
    if (!TestNotNull(TEXT("创建测试世界"), World)) { return false; }
    AActor* Instance = World->SpawnActor<AActor>(Blueprint->GeneratedClass);
    if (!TestNotNull(TEXT("在世界中创建测试实例"), Instance)) { World->DestroyWorld(false); return false; }
    GLog->AddOutputDevice(&Capture);
    {
        FEditorScriptExecutionGuard ScriptGuard;
        Instance->ProcessEvent(Function, nullptr);
    }
    // 组合运行时日志可能进入线程缓冲；在移除临时捕获器前同步派发。
    GLog->FlushThreadedLogs();
    GLog->RemoveOutputDevice(&Capture);
    World->DestroyWorld(false);
    TestTrue(TEXT("运行时打印字符串"), Capture.Message.Contains(TEXT("DebugPrint sentinel")));
    TestTrue(TEXT("CustomThunk导出真实数组内容"), Capture.Message.Contains(TEXT("7,8")));
    TestTrue(TEXT("拆分重建后的结构体保留分量"), Capture.Message.Contains(TEXT("X=12")) && Capture.Message.Contains(TEXT("Y=34")) && Capture.Message.Contains(TEXT("Z=56")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPrintFormatting, "XTools.BlueprintExtensions.DebugPrint.Formatting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDebugPrintFormatting::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("默认命名输出"), UDebugPrintLibrary::FormatDebugValues({TEXT("42"), TEXT("true")}, {TEXT("Health"), TEXT("Ready")}, true, false, TEXT(" | ")), FString(TEXT("Health = 42 | Ready = true")));
    TestEqual(TEXT("逐行且隐藏名称"), UDebugPrintLibrary::FormatDebugValues({TEXT("42"), TEXT("true")}, {}, false, true, TEXT(" | ")), FString(TEXT("42\ntrue")));
    TestEqual(TEXT("缺失名称不越界"), UDebugPrintLibrary::FormatDebugValues({TEXT("42")}, {}, true, false, TEXT(" | ")), FString(TEXT("1 = 42")));
    auto* Detached = NewObject<UK2Node_DebugPrint>();
    Detached->AllocateDefaultPins();
    TestNotNull(TEXT("脱离蓝图分配引脚不崩溃"), Detached->FindPin(TEXT("Value_0")));
    return !HasAnyErrors();
}
#endif
