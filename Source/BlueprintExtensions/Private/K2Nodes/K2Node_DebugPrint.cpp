/* Copyright (c) 2026 XIYBHK. Licensed under UE_XTools License. */
#include "K2Nodes/K2Node_DebugPrint.h"
#include "K2NodeHelpers.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Knot.h"
#include "K2Node_MakeArray.h"
#include "Libraries/DebugPrintLibrary.h"
#include "Settings/DebugPrintSettings.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "XToolsDebugPrint"

UK2Node_DebugPrint::UK2Node_DebugPrint()
{
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
    ValueLabels.SetNum(ValuePinNames.Num());

    auto AddOption = [this](FName Type, FName Name, const TCHAR* Label, const TCHAR* Default)
    {
        UEdGraphPin* Pin = CreatePin(EGPD_Input, Type, Name);
        Pin->PinFriendlyName = FText::FromString(Label);
        Pin->DefaultValue = Default;
        Pin->bAdvancedView = true;
        return Pin;
    };
    const UDebugPrintSettings* Settings = GetDefault<UDebugPrintSettings>();
    UEdGraphPin* Mode = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Byte, StaticEnum<EXToolsDebugPrintMode>(), TEXT("Mode"));
    Mode->PinFriendlyName = LOCTEXT("PrintMode", "打印模式");
    Mode->DefaultValue = StaticEnum<EXToolsDebugPrintMode>()->GetNameStringByValue(static_cast<int64>(Settings->Mode));
    Mode->PinToolTip = TEXT("单行追加、自动覆盖、逐行、带名称或列对齐。");
    AddOption(UEdGraphSchema_K2::PC_String, TEXT("Separator"), TEXT("分隔符"), *Settings->Separator);
    AddOption(UEdGraphSchema_K2::PC_Boolean, TEXT("bPrintToScreen"), TEXT("输出到屏幕"), Settings->bPrintToScreen ? TEXT("true") : TEXT("false"));
    AddOption(UEdGraphSchema_K2::PC_Boolean, TEXT("bPrintToLog"), TEXT("输出到日志"), Settings->bPrintToLog ? TEXT("true") : TEXT("false"));
    UEdGraphPin* Color = AddOption(UEdGraphSchema_K2::PC_Struct, TEXT("TextColor"), TEXT("文字颜色"), *Settings->TextColor.ToString());
    Color->PinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
    UEdGraphPin* Duration = AddOption(UEdGraphSchema_K2::PC_Real, TEXT("Duration"), TEXT("持续时间"), *FString::SanitizeFloat(Settings->Duration));
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

UEdGraphPin* UK2Node_DebugPrint::AddValuePin(const FEdGraphPinType& Type)
{
    const FName Name(*FString::Printf(TEXT("Value_%d"), NextValueId++));
    ValuePinNames.Add(Name);
    ValueLabels.SetNum(ValuePinNames.Num());
    UEdGraphPin* Pin = CreateValuePin(Name);
    Pin->PinType = Type;
    Pin->PinType.bIsReference = false;
    Pin->PinType.bIsConst = false;
    return Pin;
}

bool UK2Node_DebugPrint::CanCreateUserDefinedPin(const FEdGraphPinType& Type, EEdGraphPinDirection Direction, FText& Error)
{
    if (Direction != EGPD_Input || Type.PinCategory == UEdGraphSchema_K2::PC_Exec
        || Type.PinCategory == UEdGraphSchema_K2::PC_Delegate || Type.PinCategory == UEdGraphSchema_K2::PC_MCDelegate
        || Type.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
    {
        Error = LOCTEXT("DropNotAllowed", "请拖入已确定类型的数据输出引脚；不支持执行引脚、委托或反向连接。");
        return false;
    }
    return true;
}

UEdGraphPin* UK2Node_DebugPrint::CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> Info)
{
    // EditablePinBase 提供原生拖入入口，持久化继续使用已有稳定名称列表，避免双份 Pin 定义。
    UserDefinedPins.Remove(Info);
    FText Error;
    if (!Info.IsValid() || !CanCreateUserDefinedPin(Info->PinType, Info->DesiredPinDirection, Error)) { return nullptr; }
    Modify();
    UEdGraphPin* Pin = AddValuePin(Info->PinType);
    Pin->PinFriendlyName = FText::FromName(Info->PinName);
    NotifyChanged();
    return Pin;
}

void UK2Node_DebugPrint::RefreshLabels()
{
    TSet<FString> UsedLabels;
    for (int32 Index = 0; Index < ValuePinNames.Num(); ++Index)
    {
        UEdGraphPin* Pin = FindPin(ValuePinNames[Index]);
        if (!Pin) { continue; }
        FString Label = ValueLabels.IsValidIndex(Index) ? ValueLabels[Index] : FString();
        if (Label.IsEmpty() && Pin->LinkedTo.Num())
        {
            const UEdGraphPin* Source = Pin->LinkedTo[0];
            // 重路由的显示名刻意为空；沿输入侧查找真正的数据来源。
            TSet<const UEdGraphPin*> Visited;
            while (Source && !Visited.Contains(Source))
            {
                Visited.Add(Source);
                const UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Source->GetOwningNode());
                if (!Knot) { break; }
                const UEdGraphPin* Input = Knot->GetInputPin();
                Source = Input && Input->LinkedTo.Num() ? Input->LinkedTo[0] : nullptr;
            }
            if (Source && !Cast<UK2Node_Knot>(Source->GetOwningNode()))
            {
                Label = Source->PinName == UEdGraphSchema_K2::PN_ReturnValue
                    ? Source->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString() : Source->GetDisplayName().ToString();
                if (Label.IsEmpty()) { Label = Source->PinName.ToString(); }
                if (Label.StartsWith(TEXT("Get "))) { Label.RightChopInline(4); }
            }
        }
        if (Label.IsEmpty()) { Label = FText::Format(LOCTEXT("Value", "值 {0}"), Index + 1).ToString(); }
        const FString Base = Label;
        for (int32 Suffix = 2; UsedLabels.Contains(Label); ++Suffix) { Label = FString::Printf(TEXT("%s_%d"), *Base, Suffix); }
        UsedLabels.Add(Label);
        Pin->PinFriendlyName = FText::FromString(Label);
    }
}

void UK2Node_DebugPrint::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    if (Event.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UK2Node_DebugPrint, ValueLabels))
    {
        ValueLabels.SetNum(ValuePinNames.Num());
        RefreshLabels();
        NotifyChanged();
    }
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
        RefreshLabels();
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
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    AddValuePin(Type);
    NotifyChanged();
}

void UK2Node_DebugPrint::AddStringPin()
{
    const FScopedTransaction Transaction(LOCTEXT("AddString", "添加字符串输入"));
    Modify();
    FEdGraphPinType Type;
    Type.PinCategory = UEdGraphSchema_K2::PC_String;
    AddValuePin(Type);
    NotifyChanged();
}

void UK2Node_DebugPrint::ResetPinToWildcard(UEdGraphPin* Pin)
{
    if (!IsValuePin(Pin) || Pin->LinkedTo.Num()) { return; }
    for (const UEdGraphPin* Child : Pin->SubPins) { if (Child->LinkedTo.Num()) { return; } }
    const FScopedTransaction Transaction(LOCTEXT("ResetType", "重置为通配类型"));
    Modify();
    Pin->Modify();
    if (Pin->SubPins.Num()) { GetDefault<UEdGraphSchema_K2>()->RecombinePin(Pin->SubPins[0]); }
    Pin->PinType = FEdGraphPinType();
    Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
    Pin->DefaultValue.Reset();
    Pin->AutogeneratedDefaultValue.Reset();
    Pin->DefaultObject = nullptr;
    Pin->DefaultTextValue = FText::GetEmpty();
    NotifyChanged();
}

bool UK2Node_DebugPrint::CanRemovePin(const UEdGraphPin* Pin) const
{
    return IsValuePin(Pin);
}

void UK2Node_DebugPrint::RemoveInputPin(UEdGraphPin* Pin)
{
    if (!CanRemovePin(Pin)) { return; }
    const FScopedTransaction Transaction(LOCTEXT("RemoveValue", "移除调试值"));
    Modify();
    const int32 Index = ValuePinNames.IndexOfByKey(Pin->PinName);
    if (ValueLabels.IsValidIndex(Index)) { ValueLabels.RemoveAt(Index); }
    ValuePinNames.Remove(Pin->PinName);
    RemovePin(Pin);
    ReconstructNode();
    RefreshLabels();
    NotifyChanged();
}

void UK2Node_DebugPrint::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
    Super::GetNodeContextMenuActions(Menu, Context);
    if (Context && !Context->bIsDebugging && !Context->Pin)
    {
        Menu->AddSection(TEXT("DebugPrintAdd"), LOCTEXT("DebugPrintSection", "调试打印")).AddMenuEntry(
            TEXT("AddString"), LOCTEXT("AddString", "添加字符串输入"), LOCTEXT("StringTip", "添加可直接填写文本的输入，用于标题或固定说明。"), FSlateIcon(),
            FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_DebugPrint*>(this), &UK2Node_DebugPrint::AddStringPin)));
    }
    if (Context && !Context->bIsDebugging && IsValuePin(Context->Pin) && !Context->Pin->LinkedTo.Num()
        && Context->Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
    {
        Menu->AddSection(TEXT("DebugPrintReset"), LOCTEXT("DebugPrintSection", "调试打印")).AddMenuEntry(
            TEXT("ResetType"), LOCTEXT("ResetType", "重置为通配类型"), LOCTEXT("ResetTip", "清除未连接输入的类型和默认值，以便连接其他类型。"), FSlateIcon(),
            FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_DebugPrint*>(this), &UK2Node_DebugPrint::ResetPinToWildcard,
                const_cast<UEdGraphPin*>(Context->Pin))));
    }
    if (Context && !Context->bIsDebugging && Context->Pin && CanRemovePin(Context->Pin))
    {
        Menu->AddSection(TEXT("DebugPrint"), LOCTEXT("DebugPrintSection", "调试打印")).AddMenuEntry(
            TEXT("RemoveValue"), LOCTEXT("RemoveValue", "移除调试值"), LOCTEXT("RemoveTip", "移除此输入值。"), FSlateIcon(),
            FUIAction(FExecuteAction::CreateUObject(const_cast<UK2Node_DebugPrint*>(this), &UK2Node_DebugPrint::RemoveInputPin,
                const_cast<UEdGraphPin*>(Context->Pin))));
    }
}

void UK2Node_DebugPrint::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
    // 在拆分结构体被展开为 MakeStruct 之前固定来源名称。
    RefreshLabels();
    TArray<FString> ResolvedLabels;
    for (FName Name : ValuePinNames)
    {
        const UEdGraphPin* Pin = FindPin(Name);
        ResolvedLabels.Add(Pin ? Pin->PinFriendlyName.ToString() : FString());
    }
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
    Print->FindPin(TEXT("NodeKey"))->DefaultValue = TEXT("XTools.DebugPrint.") + NodeGuid.ToString(EGuidFormats::Digits);
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
        Labels->FindPin(Labels->GetPinName(Index))->DefaultValue = ResolvedLabels[Index];
        UEdGraphPin* Destination = Values->FindPin(Values->GetPinName(Index));
        if (Value->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard) { continue; }
        if (Value->PinType.PinCategory == UEdGraphSchema_K2::PC_String && !Value->PinType.IsContainer())
        {
            if (!CompilerContext.MovePinLinksToIntermediate(*Value, *Destination).CanSafeConnect())
            { K2NodeHelpers::ReportExpandError(CompilerContext, this, LOCTEXT("MoveValue", "无法迁移调试值")); return; }
            continue;
        }
        {
            FEdGraphPinType StringType;
            StringType.PinCategory = UEdGraphSchema_K2::PC_String;
            const auto Autocast = CompilerContext.GetSchema()->SearchForAutocastFunction(Value->PinType, StringType);
            if (Autocast.IsSet())
            {
                UFunction* Function = Autocast->FunctionOwner->FindFunctionByName(Autocast->TargetFunction);
                auto* Conversion = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
                Conversion->SetFromFunction(Function);
                Conversion->AllocateDefaultPins();
                UEdGraphPin* ConversionInput = nullptr;
                for (UEdGraphPin* Pin : Conversion->Pins)
                {
                    if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Self
                        && CompilerContext.GetSchema()->ArePinTypesCompatible(Value->PinType, Pin->PinType))
                    { ConversionInput = Pin; break; }
                }
                if (!ConversionInput || !CompilerContext.MovePinLinksToIntermediate(*Value, *ConversionInput).CanSafeConnect())
                { K2NodeHelpers::ReportExpandError(CompilerContext, this, LOCTEXT("AutocastFailed", "无法连接调试值的字符串转换函数")); return; }
                K2NodeHelpers::TryConnect(CompilerContext, Conversion->GetReturnValuePin(), Destination);
                continue;
            }
        }
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
FText UK2Node_DebugPrint::GetTooltipText() const { return LOCTEXT("Tip", "将数据输出拖到节点区域即可添加输入。支持五种打印模式、自动列对齐、详情面板自定义名称和屏幕覆盖。仅开发构建生效。"); }
FLinearColor UK2Node_DebugPrint::GetNodeTitleColor() const { return FLinearColor(0.1f, 0.55f, 0.8f); }

#undef LOCTEXT_NAMESPACE
