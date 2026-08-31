/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "K2Node_SpawnActorFromPool.h"

#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FObjectPoolEditor_SpawnNodeCompilesExpansion,
    "XTools.ObjectPoolEditor.SpawnNode.CompilesExpansion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FObjectPoolEditor_SpawnNodeCompilesExpansion::RunTest(const FString& Parameters)
{
    AddExpectedError(TEXT("ScanPathsSynchronous: Package /Game/ObjectPoolSpawnNodeTest_"),
        EAutomationExpectedErrorFlags::Contains, 1);

    const FName BlueprintName = MakeUniqueObjectName(
        GetTransientPackage(), UBlueprint::StaticClass(), TEXT("ObjectPoolSpawnNodeTest"));
    UPackage* BlueprintPackage = CreatePackage(
        *FString::Printf(TEXT("/Game/%s"), *BlueprintName.ToString()));
    BlueprintPackage->SetFlags(RF_Transient);
    UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        BlueprintPackage,
        BlueprintName,
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        NAME_None);
    UEdGraph* EventGraph = Blueprint ? FBlueprintEditorUtils::FindEventGraph(Blueprint) : nullptr;
    if (!TestNotNull(TEXT("应创建对象池节点测试蓝图"), Blueprint) ||
        !TestNotNull(TEXT("应找到对象池节点测试事件图"), EventGraph))
    {
        return false;
    }

    UK2Node_SpawnActorFromPool* SpawnNode = NewObject<UK2Node_SpawnActorFromPool>(EventGraph);
    EventGraph->AddNode(SpawnNode);
    SpawnNode->CreateNewGuid();
    SpawnNode->AllocateDefaultPins();

    UEdGraphPin* ClassPin = SpawnNode->FindPin(TEXT("Class"), EGPD_Input);
    if (!TestNotNull(TEXT("对象池生成节点应包含 Class 引脚"), ClassPin))
    {
        return false;
    }
    ClassPin->DefaultObject = AActor::StaticClass();
    SpawnNode->PinDefaultValueChanged(ClassPin);

    UEdGraphPin* ResultPin = SpawnNode->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
    TestTrue(TEXT("返回值应保持 Actor 类型"),
        ResultPin && ResultPin->PinType.PinSubCategoryObject == AActor::StaticClass());

    for (const FName PinName : {FName(TEXT("CollisionHandlingOverride")), FName(TEXT("TransformScaleMethod")), FName(TEXT("Owner"))})
    {
        if (UEdGraphPin* UnsupportedPin = SpawnNode->FindPin(PinName, EGPD_Input))
        {
            TestTrue(*FString::Printf(TEXT("不支持引脚 %s 应隐藏"), *PinName.ToString()), UnsupportedPin->bHidden);
            TestTrue(*FString::Printf(TEXT("不支持引脚 %s 应禁止新连接"), *PinName.ToString()), UnsupportedPin->bNotConnectable);
        }
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TestEqual(TEXT("对象池生成节点展开后蓝图应无警告编译成功"), Blueprint->Status, BS_UpToDate);
    return true;
}

#endif
