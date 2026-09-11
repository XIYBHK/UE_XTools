/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#include "K2Nodes/K2Node_DebugPrint.h"
#include "K2NodeHelpers.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeArray.h"
#include "Libraries/DebugPrintLibrary.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "XToolsDebugPrint"

UK2Node_DebugPrint::UK2Node_DebugPrint()
{
    ValuePinNames.Add(TEXT("Value_0"));
    NextValueId = 1;
    SetEnabledState(ENodeEnabledState::DevelopmentOnly, false);
}

UEdGraphPin* UK2Node_DebugPrint::CreateValuePin(FName Name)
{
    UEdGraphPin* Pin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, Name);
    Pin->PinFriendlyName = FText::Format(LOCTEXT("Value", "值 {0}"), ValuePinNames.IndexOfByKey(Name) + 1);
    return Pin;
}

bool UK2Node_DebugPrint::IsValuePin(const UEdGraphPin* Pin) const
{
    return Pin && !Pin->ParentPin && ValuePinNames.Contains(Pin->PinName);
}

void UK2Node_DebugPrint::AllocateDefaultPins()
{
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
    for (FName Name : ValuePinNames) { CreateValuePin(Name); }

    auto AddOption = [this](FName Type, FName Name, const TCHAR* Label, const TCHAR* Default)
    {
        UEdGraphPin* Pin = CreatePin(EGPD_Input, Type, Name);
        Pin->PinFriendlyName = FText::FromString(Label);
        Pin->DefaultValue = Default;
        Pin->bAdvancedView = true;
        return Pin;
    };
    AddOption(UEdGraphSchema_K2::PC_Boolean, TEXT("bShowLabels"), TEXT("显示名称"), TEXT("true"));
    AddOption(UEdGraphSchema_K2::PC_Boolean, TEXT("bNewLine"), TEXT("逐行显示"), TEXT("false"));
    AddOption(UEdGraphSchema_K2::PC_String, TEXT("Separator"), TEXT("分隔符"), TEXT(" | "));
    AddOption(UEdGraphSchema_K2::PC_Boolean, TEXT("bPrintToScreen"), TEXT("输出到屏幕"), TEXT("true"));
    AddOption(UEdGraphSchema_K2::PC_Boolean, TEXT("bPrintToLog"), TEXT("输出到日志"), TEXT("true"));
    UEdGraphPin* Color = AddOption(UEdGraphSchema_K2::PC_Struct, TEXT("TextColor"), TEXT("文字颜色"), TEXT("(R=0.0,G=0.66,B=1.0,A=1.0)"));
    Color->PinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
    UEdGraphPin* Duration = AddOption(UEdGraphSchema_K2::PC_Real, TEXT("Duration"), TEXT("持续时间"), TEXT("2.0"));
    Duration->PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    AddOption(UEdGraphSchema_K2::PC_Name, TEXT("Key"), TEXT("覆盖键"), TEXT("None"));
    UEdGraphPin* World = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), TEXT("WorldContextObject"));
    World->PinFriendlyName = LOCTEXT("World", "世界上下文");
    // 与原生 PrintString 一样，Actor 等支持 GetWorld 的蓝图自动使用 Self；普通 UObject 可显式传入。
    const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this);
    World->bHidden = Blueprint && Blueprint->ParentClass && Blueprint->ParentClass->GetDefaultObject()->ImplementsGetWorld();
    if (AdvancedPinDisplay == ENodeAdvancedPins::NoPins) { AdvancedPinDisplay = ENodeAdvancedPins::Hidden; }
    Super::AllocateDefaultPins();
}

void UK2Node_DebugPrint::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
    AllocateDefaultPins();
    for (const UEdGraphPin* Old : OldPins)
    {
        if (IsValuePin(Old))
        {
            if (UEdGraphPin* Pin = FindPin(Old->PinName))
            {
                Pin->PinType = Old->PinType;
                Pin->PinFriendlyName = Old->PinFriendlyName;
            }
        }
    }
    RestoreSplitPins(OldPins);
}

void UK2Node_DebugPrint::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
    Super::NotifyPinConnectionListChanged(Pin);
    if (IsValuePin(Pin) && Pin->LinkedTo.Num())
    {
        const UEdGraphPin* Source = Pin->LinkedTo[0];
        Pin->PinType = Source->PinType;
        Pin->PinType.bIsReference = false;
        Pin->PinType.bIsConst = false;
        Pin->PinFriendlyName = Source->PinName == UEdGraphSchema_K2::PN_ReturnValue
            ? Source->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView) : Source->GetDisplayName();
    }
}

bool UK2Node_DebugPrint::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
    if (IsValuePin(MyPin) && OtherPin && (OtherPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
        || OtherPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate
        || OtherPin->PinType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate))
    {
        OutReason = TEXT("调试值不能连接执行引脚或委托。");
        return true;
    }
    return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

void UK2Node_DebugPrint::NotifyChanged()
{
    if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this)) { FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint); }
    if (GetGraph()) { GetGraph()->NotifyGraphChanged(); }
}

void UK2Node_DebugPrint::AddInputPin()
{
    const FScopedTransaction Transaction(LOCTEXT("AddValue", "添加调试值"));
    Modify();
    const FName Name(*FString::Printf(TEXT("Value_%d"), NextValueId++));
    ValuePinNames.Add(Name);
    CreateValuePin(Name);
    NotifyChanged();
}

bool UK2Node_DebugPrint::CanRemovePin(const UEdGraphPin* Pin) const
{
    return IsValuePin(Pin) && ValuePinNames.Num() > 1;
}

void UK2Node_DebugPrint::RemoveInputPin(UEdGraphPin* Pin)
{
    if (!CanRemovePin(Pin)) { return; }
    const FScopedTransaction Transaction(LOCTEXT("RemoveValue", "移除调试值"));
    Modify();
    ValuePinNames.Remove(Pin->PinName);
    RemovePin(Pin);
    ReconstructNode();
    NotifyChanged();
}

void UK2Node_DebugPrint::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
    Super::GetNodeContextMenuActions(Menu, Context);
    if (Context && Context->Pin && CanRemovePin(Context->Pin))
    {
        Menu->AddSection(TEXT("DebugPrint"), LOCTEXT("DebugPrintSection", "调试打印")).AddMenuEntry(
            TEXT("RemoveValue"), LOCTEXT("RemoveValue", "移除调试值"), LOCTEXT("RemoveTip", "移除此输入值。"), FSlateIcon(),
            FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_DebugPrint*>(this), &UK2Node_DebugPrint::RemoveInputPin,
                const_cast<UEdGraphPin*>(Context->Pin))));
    }
}

void UK2Node_DebugPrint::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    Super::ExpandNode(CompilerContext, SourceGraph);
    if (!K2NodeHelpers::BeginExpandNode(CompilerContext, this, {GetExecPin(), GetThenPin()}, LOCTEXT("Missing", "调试打印缺少执行引脚"))) { return; }
    auto SpawnCall = [&](FName FunctionName)
    {
        auto* Call = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
        Call->SetFromFunction(UDebugPrintLibrary::StaticClass()->FindFunctionByName(FunctionName));
        Call->AllocateDefaultPins();
        return Call;
    };
    auto* Print = SpawnCall(GET_FUNCTION_NAME_CHECKED(UDebugPrintLibrary, PrintDebugValues));
    auto MakeStrings = [&](const TCHAR* Argument)
    {
        auto* Array = CompilerContext.SpawnIntermediateNode<UK2Node_MakeArray>(this, SourceGraph);
        Array->NumInputs = ValuePinNames.Num();
        Array->AllocateDefaultPins();
        K2NodeHelpers::TryConnect(CompilerContext, Array->GetOutputPin(), Print->FindPin(Argument));
        return Array;
    };
    auto* Values = MakeStrings(TEXT("Values"));
    auto* Labels = MakeStrings(TEXT("Labels"));
    UEdGraphPin* FirstExec = nullptr;
    UEdGraphPin* PreviousThen = nullptr;
    for (int32 Index = 0; Index < ValuePinNames.Num(); ++Index)
    {
        UEdGraphPin* Value = FindPin(ValuePinNames[Index]);
        if (!Value) { K2NodeHelpers::ReportExpandError(CompilerContext, this, LOCTEXT("MissingValue", "调试打印缺少值引脚")); return; }
        Labels->FindPin(Labels->GetPinName(Index))->DefaultValue = Value->PinFriendlyName.ToString();
        UEdGraphPin* Destination = Values->FindPin(Values->GetPinName(Index));
        if (Value->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard) { continue; }
        auto* Convert = SpawnCall(GET_FUNCTION_NAME_CHECKED(UDebugPrintLibrary, ValueToDebugString));
        UEdGraphPin* Input = Convert->FindPin(TEXT("Value"));
        Input->PinType = Value->PinType;
        // CustomThunk 读取属性地址；字面量表达式没有地址，先求值到有类型的临时变量。
        auto* Temporary = CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(this, SourceGraph);
        Temporary->VariableType = Value->PinType;
        Temporary->AllocateDefaultPins();
        auto* Assign = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(this, SourceGraph);
        Assign->AllocateDefaultPins();
        K2NodeHelpers::TryConnect(CompilerContext, Temporary->GetVariablePin(), Assign->GetVariablePin());
        if (!CompilerContext.MovePinLinksToIntermediate(*Value, *Assign->GetValuePin()).CanSafeConnect())
        { K2NodeHelpers::ReportExpandError(CompilerContext, this, LOCTEXT("MoveValue", "无法迁移调试值")); return; }
        K2NodeHelpers::TryConnect(CompilerContext, Temporary->GetVariablePin(), Input);
        if (!FirstExec) { FirstExec = Assign->GetExecPin(); }
        if (PreviousThen) { K2NodeHelpers::TryConnect(CompilerContext, PreviousThen, Assign->GetExecPin()); }
        PreviousThen = Assign->GetThenPin();
        K2NodeHelpers::TryConnect(CompilerContext, Convert->GetReturnValuePin(), Destination);
    }
    if (PreviousThen) { K2NodeHelpers::TryConnect(CompilerContext, PreviousThen, Print->GetExecPin()); }
    for (UEdGraphPin* Pin : Pins)
    {
        if (!Pin->ParentPin && !IsValuePin(Pin))
        {
            UEdGraphPin* Target = Print->FindPin(Pin->PinName);
            if (Pin == GetExecPin() && FirstExec) { Target = FirstExec; }
            if (!Target || !CompilerContext.MovePinLinksToIntermediate(*Pin, *Target).CanSafeConnect())
            { K2NodeHelpers::ReportExpandError(CompilerContext, this, LOCTEXT("MoveOption", "无法迁移调试打印参数")); return; }
        }
    }
    K2NodeHelpers::EndExpandNode(this);
}

void UK2Node_DebugPrint::GetMenuActions(FBlueprintActionDatabaseRegistrar& Registrar) const
{
    if (Registrar.IsOpenForRegistration(GetClass())) { Registrar.AddBlueprintAction(GetClass(), UBlueprintNodeSpawner::Create(GetClass())); }
}
FText UK2Node_DebugPrint::GetMenuCategory() const { return LOCTEXT("Category", "XTools|调试"); }
FText UK2Node_DebugPrint::GetNodeTitle(ENodeTitleType::Type TitleType) const { return LOCTEXT("Title", "调试打印（DebugPrint）"); }
FText UK2Node_DebugPrint::GetTooltipText() const { return LOCTEXT("Tip", "打印多个值及来源名称。添加输入可接不同类型；高级选项可设置逐行、日志和覆盖键。仅开发构建生效。"); }
FLinearColor UK2Node_DebugPrint::GetNodeTitleColor() const { return FLinearColor(0.1f, 0.55f, 0.8f); }

#undef LOCTEXT_NAMESPACE
