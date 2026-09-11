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
#include "K2Node_Knot.h"
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
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Editor.h"
#include "EdGraphUtilities.h"
#include "ScopedTransaction.h"
#include "Settings/DebugPrintSettings.h"
#include "Kismet/KismetStringLibrary.h"

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
    Print->FindPin(TEXT("bPrintToLog"))->DefaultValue = TEXT("true");
    Print->FindPin(TEXT("Mode"))->DefaultValue = TEXT("Inline");
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPrintNativeDrop, "XTools.BlueprintExtensions.DebugPrint.NativeDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDebugPrintNativeDrop::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("ScanPathsSynchronous: Package /Engine/Transient does not exist"), EAutomationExpectedErrorFlags::Contains, 1);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("DebugPrintDropTest")), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("RunDropPrint"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, true, nullptr);
    auto* Entry = FBlueprintEditorUtils::GetEntryNode(Graph);
    auto* Result = FBlueprintEditorUtils::FindOrCreateFunctionResultNode(Entry);
    const auto* Schema = GetDefault<UEdGraphSchema_K2>();
    auto* Print = NewObject<UK2Node_DebugPrint>(Graph, NAME_None, RF_Transactional);
    Graph->AddNode(Print, false, false);
    Print->CreateNewGuid();
    Print->AllocateDefaultPins();
    Print->PostPlacedNewNode();
    TestNull(TEXT("新节点无需占位输入"), Print->FindPin(TEXT("Value_0")));
    TestTrue(TEXT("拼音关键词应进入节点搜索元数据"), Print->GetKeywords().ToString().Contains(TEXT("dayin")));
    TestEqual(TEXT("新节点采用编辑器默认打印模式"), Print->FindPin(TEXT("Mode"))->DefaultValue,
        StaticEnum<EXToolsDebugPrintMode>()->GetNameStringByValue(static_cast<int64>(GetDefault<UDebugPrintSettings>()->Mode)));

    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
    Entry->CreateUserDefinedPin(TEXT("Health"), IntType, EGPD_Output);
    FText DropMessage;
    TestTrue(TEXT("数据输出可拖入节点区域"), Schema->SupportsDropPinOnNode(Print, IntType, EGPD_Output, DropMessage));
    TestFalse(TEXT("数据输入不可反向拖入"), Schema->SupportsDropPinOnNode(Print, IntType, EGPD_Input, DropMessage));
    FEdGraphPinType ExecType;
    ExecType.PinCategory = UEdGraphSchema_K2::PC_Exec;
    TestFalse(TEXT("执行线不可成为调试值"), Schema->SupportsDropPinOnNode(Print, ExecType, EGPD_Output, DropMessage));

    FName DroppedName;
    {
        const FScopedTransaction Transaction(NSLOCTEXT("XToolsDebugPrintTests", "Drop", "测试拖入调试值"));
        UEdGraphPin* Dropped = Schema->DropPinOnNode(Print, TEXT("Health"), IntType, EGPD_Output);
        if (!TestNotNull(TEXT("Schema原生拖入应创建输入"), Dropped)) { return false; }
        DroppedName = Dropped->PinName;
        TestTrue(TEXT("拖入输入应与来源自动连接"), Schema->TryCreateConnection(Entry->FindPin(TEXT("Health")), Dropped));
    }
    TestTrue(TEXT("可撤销拖入"), GEditor->UndoTransaction());
    TestNull(TEXT("撤销后删除新增输入"), Print->FindPin(DroppedName));
    TestTrue(TEXT("可重做拖入"), GEditor->RedoTransaction());
    UEdGraphPin* Restored = Print->FindPin(DroppedName);
    if (!TestNotNull(TEXT("重做恢复输入"), Restored)) { return false; }
    TestEqual(TEXT("重做恢复连线"), Restored->LinkedTo.Num(), 1);

    UEdGraphPin* Second = Schema->DropPinOnNode(Print, TEXT("Health"), IntType, EGPD_Output);
    if (!TestNotNull(TEXT("同名来源可再次拖入"), Second)) { return false; }
    TestTrue(TEXT("第二次拖入连接"), Schema->TryCreateConnection(Entry->FindPin(TEXT("Health")), Second));
    TestEqual(TEXT("重复来源名称应自动区分"), Second->PinFriendlyName.ToString(), FString(TEXT("Health_2")));
    Print->ValueLabels[0] = TEXT("HP");
    Print->ValueLabels[1] = TEXT("HP");
    FPropertyChangedEvent LabelsChanged(FindFProperty<FProperty>(UK2Node_DebugPrint::StaticClass(), TEXT("ValueLabels")));
    Print->PostEditChangeProperty(LabelsChanged);
    TestEqual(TEXT("详情自定义名称应生效"), Print->FindPin(DroppedName)->PinFriendlyName.ToString(), FString(TEXT("HP")));
    TestEqual(TEXT("重复自定义名称应区分"), Print->FindPin(TEXT("Value_1"))->PinFriendlyName.ToString(), FString(TEXT("HP_2")));

    Print->AddStringPin();
    Print->FindPin(TEXT("Value_2"))->DefaultValue = TEXT("Drop sentinel");
    FEdGraphPinType VectorType;
    VectorType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    VectorType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    Entry->CreateUserDefinedPin(TEXT("Position"), VectorType, EGPD_Output);
    UEdGraphPin* VectorInput = Schema->DropPinOnNode(Print, TEXT("Position"), VectorType, EGPD_Output);
    if (!TestNotNull(TEXT("结构体也可拖入"), VectorInput)) { return false; }
    TestTrue(TEXT("结构体输入连接"), Schema->TryCreateConnection(Entry->FindPin(TEXT("Position")), VectorInput));
    Print->FindPin(TEXT("Duration"))->DefaultValue = TEXT("9.0");
    Print->ReconstructNode();
    TestEqual(TEXT("重建不覆盖已设置的默认值"), Print->FindPin(TEXT("Duration"))->DefaultValue, FString(TEXT("9.0")));
    TestEqual(TEXT("重建保留自定义名称"), Print->FindPin(DroppedName)->PinFriendlyName.ToString(), FString(TEXT("HP")));
    TestEqual(TEXT("原生拖入不遗留双份引脚定义"), Print->UserDefinedPins.Num(), 0);

    TSet<UObject*> ToCopy;
    ToCopy.Add(Print);
    FString Clipboard;
    FEdGraphUtilities::ExportNodesToText(ToCopy, Clipboard);
    TSet<UEdGraphNode*> Imported;
    FEdGraphUtilities::ImportNodesFromText(Graph, Clipboard, Imported);
    TestEqual(TEXT("剪贴板应还原一个节点"), Imported.Num(), 1);
    for (UEdGraphNode* Node : Imported)
    {
        auto* Copy = Cast<UK2Node_DebugPrint>(Node);
        if (TestNotNull(TEXT("复制保留节点类型"), Copy))
        {
            TestEqual(TEXT("复制保留字符串值"), Copy->FindPin(TEXT("Value_2"))->DefaultValue, FString(TEXT("Drop sentinel")));
            TestEqual(TEXT("复制保留自定义标签"), Copy->ValueLabels[0], FString(TEXT("HP")));
            Copy->ResetPinToWildcard(Copy->FindPin(TEXT("Value_2")));
            TestEqual(TEXT("右键重置类型"), Copy->FindPin(TEXT("Value_2"))->PinType.PinCategory, UEdGraphSchema_K2::PC_Wildcard);
            TestTrue(TEXT("重置清理旧文本值"), Copy->FindPin(TEXT("Value_2"))->DefaultValue.IsEmpty());
            Copy->RemoveInputPin(Copy->FindPin(TEXT("Value_1")));
            TestNotNull(TEXT("删除中间输入不改变后续身份"), Copy->FindPin(TEXT("Value_3")));
        }
        Graph->RemoveNode(Node);
    }

    Print->FindPin(TEXT("Mode"))->DefaultValue = TEXT("Inline");
    Print->FindPin(TEXT("bPrintToScreen"))->DefaultValue = TEXT("false");
    Print->FindPin(TEXT("bPrintToLog"))->DefaultValue = TEXT("true");
    TestTrue(TEXT("连接执行入口"), Schema->TryCreateConnection(Entry->FindPin(UEdGraphSchema_K2::PN_Then), Print->GetExecPin()));
    TestTrue(TEXT("连接执行出口"), Schema->TryCreateConnection(Print->GetThenPin(), Result->FindPin(UEdGraphSchema_K2::PN_Execute)));
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    if (!TestTrue(TEXT("原生拖入后的蓝图应可编译"), Blueprint->Status != BS_Error) || !Blueprint->GeneratedClass) { return false; }
    UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(TEXT("RunDropPrint"));
    if (!TestNotNull(TEXT("生成执行函数"), Function)) { return false; }
    FStructOnScope Arguments(Function);
    auto* HealthProperty = FindFProperty<FIntProperty>(Function, TEXT("Health"));
    auto* PositionProperty = FindFProperty<FStructProperty>(Function, TEXT("Position"));
    if (!HealthProperty || !PositionProperty) { AddError(TEXT("测试函数缺少输入属性")); return false; }
    HealthProperty->SetPropertyValue_InContainer(Arguments.GetStructMemory(), 77);
    const FVector Position(12, 34, 56);
    PositionProperty->CopyCompleteValue(PositionProperty->ContainerPtrToValuePtr<void>(Arguments.GetStructMemory()), &Position);
    struct FCapture : FOutputDevice
    {
        FString Message;
        void Serialize(const TCHAR* Text, ELogVerbosity::Type, const FName& Category) override
        {
            if (Category == TEXT("LogBlueprintUserMessages") && FString(Text).Contains(TEXT("Drop sentinel"))) { Message = Text; }
        }
    } Capture;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DebugPrintDropWorld"));
    if (!World) { AddError(TEXT("创建测试世界失败")); return false; }
    AActor* Instance = World->SpawnActor<AActor>(Blueprint->GeneratedClass);
    if (!Instance) { World->DestroyWorld(false); AddError(TEXT("创建测试Actor失败")); return false; }
    GLog->AddOutputDevice(&Capture);
    {
        FEditorScriptExecutionGuard Guard;
        Instance->ProcessEvent(Function, Arguments.GetStructMemory());
    }
    GLog->FlushThreadedLogs();
    GLog->RemoveOutputDevice(&Capture);
    World->DestroyWorld(false);
    TestTrue(TEXT("拖入的整数使用原生字符串转换"), Capture.Message.Contains(TEXT("77 | 77 | Drop sentinel")));
    TestTrue(TEXT("向量采用UE字符串自动转换"), Capture.Message.Contains(UKismetStringLibrary::Conv_VectorToString(Position)));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDebugPrintRerouteLabels, "XTools.BlueprintExtensions.DebugPrint.RerouteLabels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDebugPrintRerouteLabels::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("ScanPathsSynchronous: Package /Engine/Transient does not exist"), EAutomationExpectedErrorFlags::Contains, 1);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), GetTransientPackage(),
        MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), TEXT("DebugPrintRerouteTest")), BPTYPE_Normal,
        UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, TEXT("RerouteLabels"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, Graph, true, nullptr);
    auto* Entry = FBlueprintEditorUtils::GetEntryNode(Graph);
    auto* Print = NewObject<UK2Node_DebugPrint>(Graph);
    Graph->AddNode(Print, false, false);
    Print->CreateNewGuid();
    Print->AllocateDefaultPins();
    const auto* Schema = GetDefault<UEdGraphSchema_K2>();
    for (int32 Index = 0; Index < 2; ++Index)
    {
        FEdGraphPinType Type;
        Type.PinCategory = Index == 0 ? UEdGraphSchema_K2::PC_Real : UEdGraphSchema_K2::PC_Object;
        if (Index == 0) { Type.PinSubCategory = UEdGraphSchema_K2::PC_Float; }
        else { Type.PinSubCategoryObject = AActor::StaticClass(); }
        const FName Name = Index == 0 ? FName(TEXT("DeltaSeconds")) : FName(TEXT("AsBP Ceshi"));
        UEdGraphPin* Source = Entry->CreateUserDefinedPin(Name, Type, EGPD_Output);
        UEdGraphPin* Previous = Source;
        for (int32 Hop = 0; Hop < 2; ++Hop)
        {
            auto* Knot = NewObject<UK2Node_Knot>(Graph);
            Graph->AddNode(Knot, false, false);
            Knot->CreateNewGuid();
            Knot->AllocateDefaultPins();
            TestTrue(TEXT("连接重路由链"), Schema->TryCreateConnection(Previous, Knot->GetInputPin()));
            Previous = Knot->GetOutputPin();
        }
        Print->AddInputPin();
        const FName ValueName(*FString::Printf(TEXT("Value_%d"), Index));
        TestTrue(TEXT("连接重路由到打印输入"), Schema->TryCreateConnection(Previous, Print->FindPin(ValueName)));
        TestEqual(TEXT("跨两级重路由读取真实来源名称"), Print->FindPin(ValueName)->PinFriendlyName.ToString(), Source->GetDisplayName().ToString());
        Print->ReconstructNode();
        TestEqual(TEXT("重建后保留来源名称"), Print->FindPin(ValueName)->PinFriendlyName.ToString(), Source->GetDisplayName().ToString());
    }
    return !HasAnyErrors();
}

#endif
