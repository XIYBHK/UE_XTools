/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "BlueprintTools/X_BlueprintGraphExporter.h"

#include "X_AssetEditor.h"

#include "Algo/Sort.h"
#include "Algo/Find.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Curves/RichCurve.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/TimelineTemplate.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "InputCoreTypes.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AddComponentByClass.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_BitmaskLiteral.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Composite.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DelegateSet.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_EnumEquality.h"
#include "K2Node_EnumInequality.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FormatText.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_GetClassDefaults.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_GetDataTableRow.h"
#include "K2Node_GetInputAxisKeyValue.h"
#include "K2Node_GetInputAxisValue.h"
#include "K2Node_GetInputVectorAxisValue.h"
#include "K2Node_GetSubsystem.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputAction.h"
#include "K2Node_InputActionEvent.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_InputAxisKeyEvent.h"
#include "K2Node_InputKey.h"
#include "K2Node_InputKeyEvent.h"
#include "K2Node_InputTouch.h"
#include "K2Node_InputTouchEvent.h"
#include "K2Node_InputVectorAxisEvent.h"
#include "K2Node_Knot.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeContainer.h"
#include "K2Node_MakeMap.h"
#include "K2Node_MakeSet.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_MathExpression.h"
#include "K2Node_MultiGate.h"
#include "K2Node_DoOnceMultiInput.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_Select.h"
#include "K2Node_SetFieldsInStruct.h"
#include "K2Node_Self.h"
#include "K2Node_SpawnActor.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_StructMemberGet.h"
#include "K2Node_StructMemberSet.h"
#include "K2Node_StructOperation.h"
#include "K2Node_Switch.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_SwitchName.h"
#include "K2Node_SwitchString.h"
#include "K2Node_Timeline.h"
#include "K2Node_Tunnel.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "X_BlueprintGraphExporter"

namespace
{
    struct FXPinRef
    {
        FString NodeId;
        FString NodeGuid;
        FString PinName;
        FString PinId;
    };

    FString SanitizeFileName(const FString& Name)
    {
        FString Result = Name;
        const TCHAR* InvalidChars = TEXT("<>:\"/\\|?*");
        for (const TCHAR* Char = InvalidChars; *Char; ++Char)
        {
            Result.ReplaceCharInline(*Char, TEXT('_'));
        }
        Result.TrimStartAndEndInline();
        return Result.IsEmpty() ? TEXT("Blueprint") : Result;
    }

    FString DirectionToString(EEdGraphPinDirection Direction)
    {
        return Direction == EGPD_Output ? TEXT("output") : TEXT("input");
    }

    bool IsExecPin(const UEdGraphPin* Pin)
    {
        return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
    }

    FString ObjectName(const UObject* Object)
    {
        return Object ? Object->GetName() : FString();
    }

    FString ObjectPathName(const UObject* Object)
    {
        return Object ? Object->GetPathName() : FString();
    }

    FString TerminalTypeToString(const FEdGraphTerminalType& TerminalType)
    {
        FString Type = TerminalType.TerminalCategory.ToString();
        if (!TerminalType.TerminalSubCategory.IsNone())
        {
            Type += TEXT(":");
            Type += TerminalType.TerminalSubCategory.ToString();
        }
        if (UObject* TypeObject = TerminalType.TerminalSubCategoryObject.Get())
        {
            Type += TEXT("/");
            Type += TypeObject->GetName();
        }
        return Type;
    }

    FString PinTypeToString(const FEdGraphPinType& PinType)
    {
        FString Type = PinType.PinCategory.ToString();
        if (!PinType.PinSubCategory.IsNone())
        {
            Type += TEXT(":");
            Type += PinType.PinSubCategory.ToString();
        }
        if (UObject* TypeObject = PinType.PinSubCategoryObject.Get())
        {
            Type += TEXT("/");
            Type += TypeObject->GetName();
        }

        switch (PinType.ContainerType)
        {
        case EPinContainerType::Array:
            return FString::Printf(TEXT("TArray<%s>"), *Type);
        case EPinContainerType::Set:
            return FString::Printf(TEXT("TSet<%s>"), *Type);
        case EPinContainerType::Map:
            return FString::Printf(TEXT("TMap<%s, %s>"), *Type, *TerminalTypeToString(PinType.PinValueType));
        default:
            return Type;
        }
    }

    FString ContainerTypeToString(EPinContainerType ContainerType)
    {
        switch (ContainerType)
        {
        case EPinContainerType::Array:
            return TEXT("array");
        case EPinContainerType::Set:
            return TEXT("set");
        case EPinContainerType::Map:
            return TEXT("map");
        default:
            return TEXT("none");
        }
    }

    FString TimelineLengthModeToString(ETimelineLengthMode LengthMode)
    {
        switch (LengthMode)
        {
        case TL_TimelineLength:
            return TEXT("timeline_length");
        case TL_LastKeyFrame:
            return TEXT("last_keyframe");
        default:
            return TEXT("unknown");
        }
    }

    FString RichCurveInterpModeToString(ERichCurveInterpMode Mode)
    {
        switch (Mode)
        {
        case RCIM_Linear:
            return TEXT("linear");
        case RCIM_Constant:
            return TEXT("constant");
        case RCIM_Cubic:
            return TEXT("cubic");
        default:
            return TEXT("none");
        }
    }

    FString RichCurveTangentModeToString(ERichCurveTangentMode Mode)
    {
        switch (Mode)
        {
        case RCTM_Auto:
            return TEXT("auto");
        case RCTM_User:
            return TEXT("user");
        case RCTM_Break:
            return TEXT("break");
        case RCTM_SmartAuto:
            return TEXT("smart_auto");
        default:
            return TEXT("none");
        }
    }

    FString RichCurveTangentWeightModeToString(ERichCurveTangentWeightMode Mode)
    {
        switch (Mode)
        {
        case RCTWM_WeightedNone:
            return TEXT("none");
        case RCTWM_WeightedArrive:
            return TEXT("arrive");
        case RCTWM_WeightedLeave:
            return TEXT("leave");
        case RCTWM_WeightedBoth:
            return TEXT("both");
        default:
            return TEXT("unknown");
        }
    }

    FString GraphTypeToString(const UBlueprint* Blueprint, const UEdGraph* Graph)
    {
        if (!Blueprint || !Graph)
        {
            return TEXT("unknown");
        }
        auto ContainsGraph = [Graph](const TArray<UEdGraph*>& Graphs)
        {
            return Algo::FindByPredicate(Graphs, [Graph](const UEdGraph* Candidate)
            {
                return Candidate == Graph;
            }) != nullptr;
        };

        if (ContainsGraph(Blueprint->UbergraphPages))
        {
            return TEXT("event");
        }
        if (ContainsGraph(Blueprint->FunctionGraphs))
        {
            return TEXT("function");
        }
        if (ContainsGraph(Blueprint->MacroGraphs))
        {
            return TEXT("macro");
        }
        return TEXT("graph");
    }

    bool HasExecOutputPin(const UEdGraphNode* Node)
    {
        if (!Node)
        {
            return false;
        }
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output && IsExecPin(Pin))
            {
                return true;
            }
        }
        return false;
    }

    bool IsTunnelEntryNode(const UK2Node_Tunnel* Tunnel)
    {
        return Tunnel && Tunnel->DrawNodeAsEntry() && HasExecOutputPin(Tunnel);
    }

    bool IsEntryNode(const UEdGraphNode* Node)
    {
        if (!Node)
        {
            return false;
        }

        if (const UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node))
        {
            return IsTunnelEntryNode(Tunnel);
        }

        if (Cast<UK2Node_FunctionEntry>(Node) ||
            Cast<UK2Node_ComponentBoundEvent>(Node) ||
            Cast<UK2Node_CustomEvent>(Node) ||
            Cast<UK2Node_Event>(Node))
        {
            return true;
        }

        const FString ClassName = Node->GetClass()->GetName();
        return ClassName.Contains(TEXT("K2Node_InputKey")) ||
            ClassName.Contains(TEXT("K2Node_InputAction")) ||
            ClassName.Contains(TEXT("K2Node_EnhancedInputAction"));
    }

    bool IsBlueprintAssetData(const FAssetData& AssetData)
    {
        UClass* AssetClass = AssetData.GetClass();
        return AssetClass && AssetClass->IsChildOf(UBlueprint::StaticClass());
    }

    bool HasExecInputPin(const UEdGraphNode* Node)
    {
        if (!Node)
        {
            return false;
        }
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Input && IsExecPin(Pin))
            {
                return true;
            }
        }
        return false;
    }

    TArray<UEdGraphNode*> GetSortedNodes(const UEdGraph* Graph)
    {
        TArray<UEdGraphNode*> Nodes = Graph ? Graph->Nodes : TArray<UEdGraphNode*>();
        Algo::Sort(Nodes, [](const UEdGraphNode* A, const UEdGraphNode* B)
        {
            if (!A || !B)
            {
                return A != nullptr;
            }
            if (A->NodePosY != B->NodePosY)
            {
                return A->NodePosY < B->NodePosY;
            }
            if (A->NodePosX != B->NodePosX)
            {
                return A->NodePosX < B->NodePosX;
            }
            return A->GetName() < B->GetName();
        });
        return Nodes;
    }

    FString NodeTitle(const UEdGraphNode* Node)
    {
        return Node ? Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : FString();
    }

    bool IsKnotNode(const UEdGraphNode* Node)
    {
        return Cast<UK2Node_Knot>(Node) != nullptr;
    }

    FString NodeTag(const UEdGraphNode* Node)
    {
        if (Cast<UK2Node_Timeline>(Node))
        {
            return TEXT(" [Timeline]");
        }
        if (Cast<UK2Node_IfThenElse>(Node))
        {
            return TEXT(" [Branch]");
        }
        if (Cast<UK2Node_ExecutionSequence>(Node))
        {
            return TEXT(" [Sequence]");
        }
        if (Cast<UK2Node_Switch>(Node))
        {
            return TEXT(" [Switch]");
        }
        if (Cast<UK2Node_Select>(Node))
        {
            return TEXT(" [Select]");
        }
        if (Cast<UK2Node_DynamicCast>(Node))
        {
            return TEXT(" [Cast]");
        }
        if (Cast<UK2Node_ConstructObjectFromClass>(Node) || Cast<UK2Node_SpawnActor>(Node))
        {
            return TEXT(" [Spawn/Construct]");
        }
        if (const UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(Node))
        {
            if (CallFunction->FunctionReference.GetMemberName() == TEXT("Delay"))
            {
                return TEXT(" [Delay]");
            }
        }
        return FString();
    }

    FString NodeLabel(const UEdGraphNode* Node, const TMap<const UEdGraphNode*, FString>& NodeIds)
    {
        FString Label = NodeTitle(Node) + NodeTag(Node);
        const FString* NodeId = NodeIds.Find(Node);
        if (!NodeId)
        {
            return Label;
        }
        return FString::Printf(TEXT("%s [%s]"), *Label, **NodeId);
    }

    void CollectPinsAfterKnot(const UEdGraphPin* Pin, TArray<const UEdGraphPin*>& OutPins, TSet<const UEdGraphNode*>& VisitedKnots)
    {
        const UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
        if (!Pin || !IsKnotNode(Node))
        {
            if (Pin)
            {
                OutPins.Add(Pin);
            }
            return;
        }

        if (VisitedKnots.Contains(Node))
        {
            return;
        }
        VisitedKnots.Add(Node);

        for (const UEdGraphPin* CandidatePin : Node->Pins)
        {
            if (!CandidatePin || CandidatePin->Direction != EGPD_Output || CandidatePin->PinType.PinCategory != Pin->PinType.PinCategory)
            {
                continue;
            }
            for (const UEdGraphPin* LinkedPin : CandidatePin->LinkedTo)
            {
                CollectPinsAfterKnot(LinkedPin, OutPins, VisitedKnots);
            }
        }
    }

    TArray<const UEdGraphPin*> ResolveDisplayPinsAfterKnot(const UEdGraphPin* Pin)
    {
        TArray<const UEdGraphPin*> Pins;
        TSet<const UEdGraphNode*> VisitedKnots;
        CollectPinsAfterKnot(Pin, Pins, VisitedKnots);
        if (Pins.Num() == 0 && Pin)
        {
            Pins.Add(Pin);
        }
        return Pins;
    }

    TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& PinType)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("display"), PinTypeToString(PinType));
        Json->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
        Json->SetStringField(TEXT("sub_category"), PinType.PinSubCategory.ToString());
        Json->SetStringField(TEXT("sub_category_object"), ObjectName(PinType.PinSubCategoryObject.Get()));
        Json->SetStringField(TEXT("sub_category_object_path"), ObjectPathName(PinType.PinSubCategoryObject.Get()));
        Json->SetStringField(TEXT("container"), ContainerTypeToString(PinType.ContainerType));
        if (PinType.ContainerType == EPinContainerType::Map)
        {
            Json->SetStringField(TEXT("value_type"), TerminalTypeToString(PinType.PinValueType));
            Json->SetStringField(TEXT("value_type_object_path"), ObjectPathName(PinType.PinValueType.TerminalSubCategoryObject.Get()));
        }
        return Json;
    }

    TSharedPtr<FJsonObject> MemberReferenceToJson(const FMemberReference& Reference)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Reference.GetMemberName().ToString());
        Json->SetStringField(TEXT("guid"), Reference.GetMemberGuid().ToString());
        if (UClass* ParentClass = Reference.GetMemberParentClass())
        {
            Json->SetStringField(TEXT("parent_class"), ParentClass->GetPathName());
        }
        return Json;
    }

    TSharedPtr<FJsonObject> SemanticPinToJson(const UEdGraphPin* Pin)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Pin)
        {
            return Json;
        }

        Json->SetStringField(TEXT("name"), Pin->PinName.ToString());
        Json->SetStringField(TEXT("id"), Pin->PinId.ToString());
        Json->SetStringField(TEXT("type"), PinTypeToString(Pin->PinType));
        if (!Pin->GetDefaultAsString().IsEmpty())
        {
            Json->SetStringField(TEXT("default"), Pin->GetDefaultAsString());
        }
        if (Pin->DefaultObject)
        {
            Json->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
        }
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> SemanticPinsToJson(const TArray<UEdGraphPin*>& Pins)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const UEdGraphPin* Pin : Pins)
        {
            if (Pin)
            {
                Values.Add(MakeShared<FJsonValueObject>(SemanticPinToJson(Pin)));
            }
        }
        return Values;
    }

    TArray<TSharedPtr<FJsonValue>> ExecOutputPinsToJson(const UEdGraphNode* Node, const FName PinNameToSkip = NAME_None)
    {
        TArray<TSharedPtr<FJsonValue>> Pins;
        if (!Node)
        {
            return Pins;
        }

        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin) || Pin->PinName == PinNameToSkip)
            {
                continue;
            }
            Pins.Add(MakeShared<FJsonValueObject>(SemanticPinToJson(Pin)));
        }
        return Pins;
    }

    TArray<TSharedPtr<FJsonValue>> NamesToJson(const TArray<FName>& Names)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const FName& Name : Names)
        {
            Values.Add(MakeShared<FJsonValueString>(Name.ToString()));
        }
        return Values;
    }

    TSharedPtr<FJsonObject> LocalVariableToJson(const FBPVariableDescription& Variable)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Variable.VarName.ToString());
        Json->SetStringField(TEXT("guid"), Variable.VarGuid.ToString());
        Json->SetObjectField(TEXT("type"), PinTypeToJson(Variable.VarType));
        if (!Variable.DefaultValue.IsEmpty())
        {
            Json->SetStringField(TEXT("default"), Variable.DefaultValue);
        }
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> LocalVariablesToJson(const TArray<FBPVariableDescription>& Variables)
    {
        TArray<TSharedPtr<FJsonValue>> Values;
        for (const FBPVariableDescription& Variable : Variables)
        {
            Values.Add(MakeShared<FJsonValueObject>(LocalVariableToJson(Variable)));
        }
        return Values;
    }

    void AddFunctionFlagsToJson(TSharedPtr<FJsonObject> Json, const UFunction* Function)
    {
        if (!Json || !Function)
        {
            return;
        }

        Json->SetBoolField(TEXT("is_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
        Json->SetBoolField(TEXT("is_const"), Function->HasAnyFunctionFlags(FUNC_Const));
        Json->SetBoolField(TEXT("is_static"), Function->HasAnyFunctionFlags(FUNC_Static));
        Json->SetBoolField(TEXT("is_latent"), Function->HasMetaData(TEXT("Latent")));
        Json->SetBoolField(TEXT("is_server_rpc"), Function->HasAnyFunctionFlags(FUNC_NetServer));
        Json->SetBoolField(TEXT("is_client_rpc"), Function->HasAnyFunctionFlags(FUNC_NetClient));
        Json->SetBoolField(TEXT("is_net_multicast"), Function->HasAnyFunctionFlags(FUNC_NetMulticast));
        Json->SetBoolField(TEXT("is_reliable"), Function->HasAnyFunctionFlags(FUNC_NetReliable));
    }

    void AddInputBindingFlagsToJson(TSharedPtr<FJsonObject> Json, bool bConsumeInput, bool bExecuteWhenPaused, bool bOverrideParentBinding)
    {
        if (!Json)
        {
            return;
        }

        Json->SetBoolField(TEXT("consume_input"), bConsumeInput);
        Json->SetBoolField(TEXT("execute_when_paused"), bExecuteWhenPaused);
        Json->SetBoolField(TEXT("override_parent_binding"), bOverrideParentBinding);
    }

    void AddKeyToJson(TSharedPtr<FJsonObject> Json, const FString& FieldName, const FKey& Key)
    {
        if (Json && Key.IsValid())
        {
            Json->SetStringField(FieldName, Key.ToString());
        }
    }

    void AddInputEventToJson(TSharedPtr<FJsonObject> Json, int64 InputEventValue)
    {
        if (!Json)
        {
            return;
        }

        Json->SetNumberField(TEXT("input_event"), InputEventValue);
        if (const UEnum* InputEventEnum = StaticEnum<EInputEvent>())
        {
            Json->SetStringField(TEXT("input_event_name"), InputEventEnum->GetNameStringByValue(InputEventValue));
        }
    }

    void AddMulticastDelegateToJson(TSharedPtr<FJsonObject> Json, const UK2Node_BaseMCDelegate* DelegateNode)
    {
        if (!Json || !DelegateNode)
        {
            return;
        }

        Json->SetObjectField(TEXT("delegate"), MemberReferenceToJson(DelegateNode->DelegateReference));
        Json->SetObjectField(TEXT("delegate_pin"), SemanticPinToJson(DelegateNode->GetDelegatePin()));
        Json->SetBoolField(TEXT("authority_only"), DelegateNode->IsAuthorityOnly());
        if (FProperty* Property = DelegateNode->GetProperty())
        {
            Json->SetStringField(TEXT("resolved_property"), Property->GetPathName());
        }
        if (UFunction* Signature = DelegateNode->GetDelegateSignature())
        {
            Json->SetStringField(TEXT("delegate_signature"), Signature->GetPathName());
        }
    }

    bool TrySetReflectedNameField(TSharedPtr<FJsonObject> Json, const UObject* Object, const FName PropertyName, const FString& FieldName)
    {
        const FNameProperty* NameProperty = Object ? FindFProperty<FNameProperty>(Object->GetClass(), PropertyName) : nullptr;
        if (!Json || !Object || !NameProperty)
        {
            return false;
        }

        Json->SetStringField(FieldName, NameProperty->GetPropertyValue_InContainer(Object).ToString());
        return true;
    }

    bool TrySetReflectedClassField(TSharedPtr<FJsonObject> Json, const UObject* Object, const FName PropertyName, const FString& FieldName)
    {
        const FObjectProperty* ObjectProperty = Object ? FindFProperty<FObjectProperty>(Object->GetClass(), PropertyName) : nullptr;
        if (!Json || !Object || !ObjectProperty)
        {
            return false;
        }

        if (const UObject* Value = ObjectProperty->GetObjectPropertyValue_InContainer(Object))
        {
            Json->SetStringField(FieldName, Value->GetPathName());
            return true;
        }
        return false;
    }

    TSharedPtr<FJsonObject> NodeSemanticToJson(const UEdGraphNode* Node)
    {
        if (!Node)
        {
            return nullptr;
        }

        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (const UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("comment"));
            Json->SetStringField(TEXT("color"), Comment->CommentColor.ToString());
            Json->SetNumberField(TEXT("font_size"), Comment->FontSize);
            Json->SetBoolField(TEXT("bubble_visible"), Comment->bCommentBubbleVisible_InDetailsPanel != 0);
            Json->SetBoolField(TEXT("bubble_uses_comment_color"), Comment->bColorCommentBubble != 0);
            Json->SetStringField(TEXT("move_mode"), Comment->MoveMode == ECommentBoxMode::GroupMovement ? TEXT("group_movement") : TEXT("comment_only"));
            Json->SetNumberField(TEXT("depth"), Comment->CommentDepth);
            Json->SetNumberField(TEXT("width"), Comment->NodeWidth);
            Json->SetNumberField(TEXT("height"), Comment->NodeHeight);
        }
        else if (const UK2Node_ConstructObjectFromClass* ConstructObject = Cast<UK2Node_ConstructObjectFromClass>(Node))
        {
            if (Cast<UK2Node_AddComponentByClass>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("add_component_by_class"));
            }
            else
            {
                Json->SetStringField(TEXT("kind"), Cast<UK2Node_SpawnActorFromClass>(Node) ? TEXT("spawn_actor") : TEXT("construct_object"));
            }
            if (const UEdGraphPin* ClassPin = ConstructObject->GetClassPin())
            {
                Json->SetObjectField(TEXT("class_pin"), SemanticPinToJson(ClassPin));
            }
            if (UClass* ClassToSpawn = ConstructObject->GetClassToSpawn())
            {
                Json->SetStringField(TEXT("target_class"), ClassToSpawn->GetPathName());
            }
            if (const UEdGraphPin* ResultPin = ConstructObject->GetResultPin())
            {
                Json->SetObjectField(TEXT("result_pin"), SemanticPinToJson(ResultPin));
            }
        }
        else if (const UK2Node_SpawnActor* SpawnActor = Cast<UK2Node_SpawnActor>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("spawn_actor"));
            if (const UEdGraphPin* BlueprintPin = SpawnActor->GetBlueprintPin())
            {
                Json->SetObjectField(TEXT("blueprint_pin"), SemanticPinToJson(BlueprintPin));
                if (BlueprintPin->DefaultObject)
                {
                    Json->SetStringField(TEXT("target_blueprint"), BlueprintPin->DefaultObject->GetPathName());
                }
            }
            if (const UEdGraphPin* ResultPin = SpawnActor->GetResultPin())
            {
                Json->SetObjectField(TEXT("result_pin"), SemanticPinToJson(ResultPin));
                if (UObject* ResultType = ResultPin->PinType.PinSubCategoryObject.Get())
                {
                    Json->SetStringField(TEXT("result_type"), ResultType->GetPathName());
                }
            }
        }
        else if (const UK2Node_DynamicCast* DynamicCast = Cast<UK2Node_DynamicCast>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("dynamic_cast"));
            Json->SetBoolField(TEXT("is_pure"), DynamicCast->IsNodePure());
            if (UClass* TargetType = DynamicCast->TargetType.Get())
            {
                Json->SetStringField(TEXT("target_type"), TargetType->GetPathName());
            }
            if (const UEdGraphPin* SourcePin = DynamicCast->GetCastSourcePin())
            {
                Json->SetObjectField(TEXT("source_pin"), SemanticPinToJson(SourcePin));
            }
            if (const UEdGraphPin* ResultPin = DynamicCast->GetCastResultPin())
            {
                Json->SetObjectField(TEXT("result_pin"), SemanticPinToJson(ResultPin));
            }
            if (const UEdGraphPin* SuccessPin = DynamicCast->GetBoolSuccessPin())
            {
                Json->SetObjectField(TEXT("success_pin"), SemanticPinToJson(SuccessPin));
            }
        }
        else if (const UK2Node_GetArrayItem* GetArrayItem = Cast<UK2Node_GetArrayItem>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("array_get"));
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetObjectField(TEXT("array_pin"), SemanticPinToJson(GetArrayItem->GetTargetArrayPin()));
            Json->SetObjectField(TEXT("index_pin"), SemanticPinToJson(GetArrayItem->GetIndexPin()));
            Json->SetObjectField(TEXT("result_pin"), SemanticPinToJson(GetArrayItem->GetResultPin()));
        }
        else if (const UK2Node_MakeContainer* MakeContainer = Cast<UK2Node_MakeContainer>(Node))
        {
            if (Cast<UK2Node_MakeMap>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("make_map"));
            }
            else if (Cast<UK2Node_MakeSet>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("make_set"));
            }
            else
            {
                Json->SetStringField(TEXT("kind"), TEXT("make_array"));
            }
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetNumberField(TEXT("input_count"), MakeContainer->NumInputs);
            Json->SetObjectField(TEXT("output_pin"), SemanticPinToJson(MakeContainer->GetOutputPin()));
        }
        else if (const UK2Node_EnumEquality* EnumEquality = Cast<UK2Node_EnumEquality>(Node))
        {
            Json->SetStringField(TEXT("kind"), Cast<UK2Node_EnumInequality>(Node) ? TEXT("enum_inequality") : TEXT("enum_equality"));
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetObjectField(TEXT("a_pin"), SemanticPinToJson(EnumEquality->GetInput1Pin()));
            Json->SetObjectField(TEXT("b_pin"), SemanticPinToJson(EnumEquality->GetInput2Pin()));
            Json->SetObjectField(TEXT("return_pin"), SemanticPinToJson(EnumEquality->GetReturnValuePin()));
            FName FunctionName = NAME_None;
            UClass* FunctionClass = nullptr;
            EnumEquality->GetConditionalFunction(FunctionName, &FunctionClass);
            Json->SetStringField(TEXT("function_name"), FunctionName.ToString());
            if (FunctionClass)
            {
                Json->SetStringField(TEXT("function_class"), FunctionClass->GetPathName());
            }
        }
        else if (const UK2Node_CastByteToEnum* CastByteToEnum = Cast<UK2Node_CastByteToEnum>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("cast_byte_to_enum"));
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetBoolField(TEXT("safe"), CastByteToEnum->bSafe);
            Json->SetStringField(TEXT("function_name"), CastByteToEnum->GetFunctionName().ToString());
            if (UEnum* Enum = CastByteToEnum->GetEnum())
            {
                Json->SetStringField(TEXT("enum_path"), Enum->GetPathName());
            }
        }
        else if (const UK2Node_EnumLiteral* EnumLiteral = Cast<UK2Node_EnumLiteral>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("enum_literal"));
            Json->SetBoolField(TEXT("is_pure"), true);
            if (UEnum* Enum = EnumLiteral->GetEnum())
            {
                Json->SetStringField(TEXT("enum_path"), Enum->GetPathName());
            }
        }
        else if (const UK2Node_BitmaskLiteral* BitmaskLiteral = Cast<UK2Node_BitmaskLiteral>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("bitmask_literal"));
            Json->SetBoolField(TEXT("is_pure"), true);
            if (UEnum* Enum = BitmaskLiteral->GetEnum())
            {
                Json->SetStringField(TEXT("enum_path"), Enum->GetPathName());
            }
        }
        else if (const UK2Node_Composite* Composite = Cast<UK2Node_Composite>(Node))
        {
            Json->SetStringField(TEXT("kind"), Cast<UK2Node_MathExpression>(Node) ? TEXT("math_expression") : TEXT("composite"));
            if (UEdGraph* BoundGraph = Composite->BoundGraph)
            {
                Json->SetStringField(TEXT("bound_graph"), BoundGraph->GetPathName());
                Json->SetStringField(TEXT("bound_graph_name"), BoundGraph->GetName());
            }
            if (const UK2Node_MathExpression* MathExpression = Cast<UK2Node_MathExpression>(Node))
            {
                Json->SetStringField(TEXT("expression"), MathExpression->Expression);
            }
        }
        else if (const UK2Node_Switch* SwitchNode = Cast<UK2Node_Switch>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("switch"));
            Json->SetBoolField(TEXT("has_default_pin"), SwitchNode->bHasDefaultPin != 0);
            Json->SetStringField(TEXT("function_name"), SwitchNode->FunctionName.ToString());
            if (UClass* FunctionClass = SwitchNode->FunctionClass.Get())
            {
                Json->SetStringField(TEXT("function_class"), FunctionClass->GetPathName());
            }
            if (const UEdGraphPin* SelectionPin = SwitchNode->GetSelectionPin())
            {
                Json->SetObjectField(TEXT("selection_pin"), SemanticPinToJson(SelectionPin));
            }

            const UEdGraphPin* DefaultPin = SwitchNode->GetDefaultPin();
            if (DefaultPin)
            {
                Json->SetObjectField(TEXT("default_pin"), SemanticPinToJson(DefaultPin));
            }
            Json->SetArrayField(TEXT("case_pins"), ExecOutputPinsToJson(Node, DefaultPin ? DefaultPin->PinName : NAME_None));

            if (const UK2Node_SwitchEnum* EnumSwitch = Cast<UK2Node_SwitchEnum>(Node))
            {
                Json->SetStringField(TEXT("switch_type"), TEXT("enum"));
                if (EnumSwitch->Enum)
                {
                    Json->SetStringField(TEXT("enum_path"), EnumSwitch->Enum->GetPathName());
                }
                Json->SetArrayField(TEXT("cases"), NamesToJson(EnumSwitch->EnumEntries));
            }
            else if (const UK2Node_SwitchInteger* IntegerSwitch = Cast<UK2Node_SwitchInteger>(Node))
            {
                Json->SetStringField(TEXT("switch_type"), TEXT("integer"));
                Json->SetNumberField(TEXT("start_index"), IntegerSwitch->StartIndex);
            }
            else if (const UK2Node_SwitchName* NameSwitch = Cast<UK2Node_SwitchName>(Node))
            {
                Json->SetStringField(TEXT("switch_type"), TEXT("name"));
                Json->SetArrayField(TEXT("cases"), NamesToJson(NameSwitch->PinNames));
            }
            else if (const UK2Node_SwitchString* StringSwitch = Cast<UK2Node_SwitchString>(Node))
            {
                Json->SetStringField(TEXT("switch_type"), TEXT("string"));
                Json->SetBoolField(TEXT("case_sensitive"), StringSwitch->bIsCaseSensitive != 0);
                Json->SetArrayField(TEXT("cases"), NamesToJson(StringSwitch->PinNames));
            }
        }
        else if (const UK2Node_Select* SelectNode = Cast<UK2Node_Select>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("select"));
            if (const UEdGraphPin* IndexPin = SelectNode->GetIndexPin())
            {
                Json->SetObjectField(TEXT("index_pin"), SemanticPinToJson(IndexPin));
            }
            if (const UEdGraphPin* ReturnPin = SelectNode->GetReturnValuePin())
            {
                Json->SetObjectField(TEXT("return_pin"), SemanticPinToJson(ReturnPin));
            }
            if (UEnum* Enum = SelectNode->GetEnum())
            {
                Json->SetStringField(TEXT("enum_path"), Enum->GetPathName());
            }

            TArray<UEdGraphPin*> OptionPins;
            SelectNode->GetOptionPins(OptionPins);
            Json->SetNumberField(TEXT("option_count"), OptionPins.Num());
            Json->SetArrayField(TEXT("option_pins"), SemanticPinsToJson(OptionPins));
        }
        else if (const UK2Node_GetDataTableRow* DataTableRow = Cast<UK2Node_GetDataTableRow>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("get_data_table_row"));
            Json->SetObjectField(TEXT("data_table_pin"), SemanticPinToJson(DataTableRow->GetDataTablePin()));
            Json->SetObjectField(TEXT("row_name_pin"), SemanticPinToJson(DataTableRow->GetRowNamePin()));
            Json->SetObjectField(TEXT("result_pin"), SemanticPinToJson(DataTableRow->GetResultPin()));
            Json->SetObjectField(TEXT("then_pin"), SemanticPinToJson(DataTableRow->GetThenPin()));
            Json->SetObjectField(TEXT("row_not_found_pin"), SemanticPinToJson(DataTableRow->GetRowNotFoundPin()));
            if (UScriptStruct* RowStruct = DataTableRow->GetDataTableRowStructType())
            {
                Json->SetStringField(TEXT("row_struct"), RowStruct->GetPathName());
            }
        }
        else if (const UK2Node_BaseAsyncTask* AsyncTask = Cast<UK2Node_BaseAsyncTask>(Node))
        {
            Json->SetStringField(TEXT("kind"), Cast<UK2Node_AsyncAction>(Node) ? TEXT("async_action") : TEXT("async_task"));
            TrySetReflectedNameField(Json, AsyncTask, TEXT("ProxyFactoryFunctionName"), TEXT("proxy_factory_function"));
            TrySetReflectedClassField(Json, AsyncTask, TEXT("ProxyFactoryClass"), TEXT("proxy_factory_class"));
            TrySetReflectedClassField(Json, AsyncTask, TEXT("ProxyClass"), TEXT("proxy_class"));
            TrySetReflectedNameField(Json, AsyncTask, TEXT("ProxyActivateFunctionName"), TEXT("proxy_activate_function"));
            Json->SetArrayField(TEXT("exec_output_pins"), ExecOutputPinsToJson(Node));

            TArray<TSharedPtr<FJsonValue>> DelegateOutputPins;
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->PinName != UEdGraphSchema_K2::PN_Then)
                {
                    DelegateOutputPins.Add(MakeShared<FJsonValueObject>(SemanticPinToJson(Pin)));
                }
            }
            Json->SetArrayField(TEXT("delegate_output_pins"), DelegateOutputPins);
        }
        else if (const UK2Node_GetClassDefaults* ClassDefaults = Cast<UK2Node_GetClassDefaults>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("get_class_defaults"));
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetObjectField(TEXT("class_pin"), SemanticPinToJson(ClassDefaults->FindClassPin()));
            if (UClass* InputClass = ClassDefaults->GetInputClass())
            {
                Json->SetStringField(TEXT("input_class"), InputClass->GetPathName());
            }
        }
        else if (const UK2Node_GetSubsystem* GetSubsystem = Cast<UK2Node_GetSubsystem>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("get_subsystem"));
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetObjectField(TEXT("class_pin"), SemanticPinToJson(GetSubsystem->GetClassPin()));
            Json->SetObjectField(TEXT("world_context_pin"), SemanticPinToJson(GetSubsystem->GetWorldContextPin()));
            Json->SetObjectField(TEXT("result_pin"), SemanticPinToJson(GetSubsystem->GetResultPin()));
        }
        else if (const UK2Node_GetInputVectorAxisValue* GetVectorAxisValue = Cast<UK2Node_GetInputVectorAxisValue>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("get_input_vector_axis_value"));
            AddKeyToJson(Json, TEXT("axis_key"), GetVectorAxisValue->InputAxisKey);
            AddInputBindingFlagsToJson(Json, GetVectorAxisValue->bConsumeInput != 0, GetVectorAxisValue->bExecuteWhenPaused != 0, false);
        }
        else if (const UK2Node_GetInputAxisKeyValue* GetAxisKeyValue = Cast<UK2Node_GetInputAxisKeyValue>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("get_input_axis_key_value"));
            AddKeyToJson(Json, TEXT("axis_key"), GetAxisKeyValue->InputAxisKey);
            AddInputBindingFlagsToJson(Json, GetAxisKeyValue->bConsumeInput != 0, GetAxisKeyValue->bExecuteWhenPaused != 0, false);
        }
        else if (const UK2Node_GetInputAxisValue* GetAxisValue = Cast<UK2Node_GetInputAxisValue>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("get_input_axis_value"));
            Json->SetStringField(TEXT("input_axis"), GetAxisValue->InputAxisName.ToString());
            AddInputBindingFlagsToJson(Json, GetAxisValue->bConsumeInput != 0, GetAxisValue->bExecuteWhenPaused != 0, false);
        }
        else if (const UK2Node_FormatText* FormatText = Cast<UK2Node_FormatText>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("format_text"));
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetObjectField(TEXT("format_pin"), SemanticPinToJson(FormatText->GetFormatPin()));
            Json->SetNumberField(TEXT("argument_count"), FormatText->GetArgumentCount());
            TArray<TSharedPtr<FJsonValue>> Arguments;
            for (int32 Index = 0; Index < FormatText->GetArgumentCount(); ++Index)
            {
                TSharedPtr<FJsonObject> ArgumentJson = MakeShared<FJsonObject>();
                const FName ArgumentName(*FormatText->GetArgumentName(Index).ToString());
                ArgumentJson->SetStringField(TEXT("name"), ArgumentName.ToString());
                ArgumentJson->SetObjectField(TEXT("pin"), SemanticPinToJson(FormatText->FindArgumentPin(ArgumentName)));
                Arguments.Add(MakeShared<FJsonValueObject>(ArgumentJson));
            }
            Json->SetArrayField(TEXT("arguments"), Arguments);
        }
        else if (const UK2Node_CallFunction* CallFunction = Cast<UK2Node_CallFunction>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("call_function"));
            Json->SetObjectField(TEXT("function"), MemberReferenceToJson(CallFunction->FunctionReference));
            if (UFunction* Function = CallFunction->GetTargetFunction())
            {
                Json->SetStringField(TEXT("resolved_function"), Function->GetPathName());
                AddFunctionFlagsToJson(Json, Function);
            }
        }
        else if (const UK2Node_CallDelegate* CallDelegate = Cast<UK2Node_CallDelegate>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("call_delegate"));
            AddMulticastDelegateToJson(Json, CallDelegate);
        }
        else if (const UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(Node))
        {
            if (Cast<UK2Node_AssignDelegate>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("assign_delegate"));
            }
            else if (Cast<UK2Node_AddDelegate>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("add_delegate"));
            }
            else if (Cast<UK2Node_RemoveDelegate>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("remove_delegate"));
            }
            else if (Cast<UK2Node_ClearDelegate>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("clear_delegate"));
            }
            else
            {
                Json->SetStringField(TEXT("kind"), TEXT("multicast_delegate"));
            }
            AddMulticastDelegateToJson(Json, DelegateNode);
        }
        else if (const UK2Node_CreateDelegate* CreateDelegate = Cast<UK2Node_CreateDelegate>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("create_delegate"));
            Json->SetBoolField(TEXT("is_pure"), true);
            Json->SetStringField(TEXT("selected_function_name"), CreateDelegate->SelectedFunctionName.ToString());
            Json->SetStringField(TEXT("selected_function_guid"), CreateDelegate->SelectedFunctionGuid.ToString());
            Json->SetObjectField(TEXT("delegate_out_pin"), SemanticPinToJson(CreateDelegate->GetDelegateOutPin()));
            Json->SetObjectField(TEXT("object_in_pin"), SemanticPinToJson(CreateDelegate->GetObjectInPin()));
            if (UFunction* Signature = CreateDelegate->GetDelegateSignature())
            {
                Json->SetStringField(TEXT("delegate_signature"), Signature->GetPathName());
            }
            if (UClass* ScopeClass = CreateDelegate->GetScopeClass())
            {
                Json->SetStringField(TEXT("scope_class"), ScopeClass->GetPathName());
            }
        }
        else if (const UK2Node_DelegateSet* DelegateSet = Cast<UK2Node_DelegateSet>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("delegate_set"));
            Json->SetStringField(TEXT("delegate_property"), DelegateSet->DelegatePropertyName.ToString());
            if (UClass* DelegateClass = DelegateSet->DelegatePropertyClass.Get())
            {
                Json->SetStringField(TEXT("delegate_property_class"), DelegateClass->GetPathName());
            }
            Json->SetObjectField(TEXT("delegate_owner_pin"), SemanticPinToJson(DelegateSet->GetDelegateOwner()));
            Json->SetStringField(TEXT("entry_point_name"), DelegateSet->GetDelegateTargetEntryPointName().ToString());
            if (UFunction* Signature = DelegateSet->GetDelegateSignature())
            {
                Json->SetStringField(TEXT("delegate_signature"), Signature->GetPathName());
            }
        }
        else if (const UK2Node_StructOperation* StructOperation = Cast<UK2Node_StructOperation>(Node))
        {
            if (Cast<UK2Node_BreakStruct>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("break_struct"));
            }
            else if (Cast<UK2Node_SetFieldsInStruct>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("set_fields_in_struct"));
            }
            else if (Cast<UK2Node_MakeStruct>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("make_struct"));
            }
            else if (Cast<UK2Node_StructMemberGet>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("struct_member_get"));
            }
            else if (Cast<UK2Node_StructMemberSet>(Node))
            {
                Json->SetStringField(TEXT("kind"), TEXT("struct_member_set"));
            }
            else
            {
                Json->SetStringField(TEXT("kind"), TEXT("struct_operation"));
            }

            Json->SetBoolField(TEXT("is_pure"), StructOperation->IsNodePure());
            Json->SetObjectField(TEXT("variable"), MemberReferenceToJson(StructOperation->VariableReference));
            if (UScriptStruct* StructType = StructOperation->StructType.Get())
            {
                Json->SetStringField(TEXT("struct_type"), StructType->GetPathName());
            }
        }
        else if (Cast<UK2Node_Self>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("self"));
            Json->SetBoolField(TEXT("is_pure"), true);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Output)
                {
                    Json->SetObjectField(TEXT("self_pin"), SemanticPinToJson(Pin));
                    break;
                }
            }
        }
        else if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("variable"));
            Json->SetObjectField(TEXT("variable"), MemberReferenceToJson(Variable->VariableReference));
            if (Cast<UK2Node_VariableGet>(Node))
            {
                Json->SetStringField(TEXT("access"), TEXT("get"));
            }
            else if (Cast<UK2Node_VariableSet>(Node))
            {
                Json->SetStringField(TEXT("access"), TEXT("set"));
            }
        }
        else if (const UK2Node_ComponentBoundEvent* ComponentEvent = Cast<UK2Node_ComponentBoundEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("component_bound_event"));
            Json->SetObjectField(TEXT("event"), MemberReferenceToJson(ComponentEvent->EventReference));
            Json->SetStringField(TEXT("delegate_property"), ComponentEvent->DelegatePropertyName.ToString());
            Json->SetStringField(TEXT("component_property"), ComponentEvent->ComponentPropertyName.ToString());
            if (FMulticastDelegateProperty* DelegateProperty = ComponentEvent->GetTargetDelegateProperty())
            {
                Json->SetStringField(TEXT("resolved_delegate"), DelegateProperty->GetPathName());
            }
        }
        else if (const UK2Node_ActorBoundEvent* ActorEvent = Cast<UK2Node_ActorBoundEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("actor_bound_event"));
            Json->SetObjectField(TEXT("event"), MemberReferenceToJson(ActorEvent->EventReference));
            Json->SetStringField(TEXT("delegate_property"), ActorEvent->DelegatePropertyName.ToString());
            if (UClass* DelegateOwnerClass = ActorEvent->DelegateOwnerClass.Get())
            {
                Json->SetStringField(TEXT("delegate_owner_class"), DelegateOwnerClass->GetPathName());
            }
            if (AActor* EventOwner = ActorEvent->EventOwner.Get())
            {
                Json->SetStringField(TEXT("event_owner"), EventOwner->GetPathName());
            }
            if (FMulticastDelegateProperty* DelegateProperty = ActorEvent->GetTargetDelegateProperty())
            {
                Json->SetStringField(TEXT("resolved_delegate"), DelegateProperty->GetPathName());
            }
        }
        else if (const UK2Node_InputActionEvent* InputActionEvent = Cast<UK2Node_InputActionEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_action_event"));
            Json->SetStringField(TEXT("input_action"), InputActionEvent->InputActionName.ToString());
            Json->SetNumberField(TEXT("input_event"), InputActionEvent->InputKeyEvent.GetValue());
            if (const UEnum* InputEventEnum = StaticEnum<EInputEvent>())
            {
                Json->SetStringField(TEXT("input_event_name"), InputEventEnum->GetNameStringByValue(InputActionEvent->InputKeyEvent.GetValue()));
            }
            AddInputBindingFlagsToJson(Json, InputActionEvent->bConsumeInput != 0, InputActionEvent->bExecuteWhenPaused != 0, InputActionEvent->bOverrideParentBinding != 0);
        }
        else if (const UK2Node_InputAxisEvent* InputAxisEvent = Cast<UK2Node_InputAxisEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_axis_event"));
            Json->SetStringField(TEXT("input_axis"), InputAxisEvent->InputAxisName.ToString());
            AddInputBindingFlagsToJson(Json, InputAxisEvent->bConsumeInput != 0, InputAxisEvent->bExecuteWhenPaused != 0, InputAxisEvent->bOverrideParentBinding != 0);
        }
        else if (const UK2Node_InputVectorAxisEvent* InputVectorAxisEvent = Cast<UK2Node_InputVectorAxisEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_vector_axis_event"));
            AddKeyToJson(Json, TEXT("axis_key"), InputVectorAxisEvent->AxisKey);
            AddInputBindingFlagsToJson(Json, InputVectorAxisEvent->bConsumeInput != 0, InputVectorAxisEvent->bExecuteWhenPaused != 0, InputVectorAxisEvent->bOverrideParentBinding != 0);
        }
        else if (const UK2Node_InputAxisKeyEvent* InputAxisKeyEvent = Cast<UK2Node_InputAxisKeyEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_axis_key_event"));
            AddKeyToJson(Json, TEXT("axis_key"), InputAxisKeyEvent->AxisKey);
            AddInputBindingFlagsToJson(Json, InputAxisKeyEvent->bConsumeInput != 0, InputAxisKeyEvent->bExecuteWhenPaused != 0, InputAxisKeyEvent->bOverrideParentBinding != 0);
        }
        else if (const UK2Node_InputAction* InputAction = Cast<UK2Node_InputAction>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_action"));
            Json->SetStringField(TEXT("input_action"), InputAction->InputActionName.ToString());
            AddInputBindingFlagsToJson(Json, InputAction->bConsumeInput != 0, InputAction->bExecuteWhenPaused != 0, InputAction->bOverrideParentBinding != 0);
            Json->SetObjectField(TEXT("pressed_pin"), SemanticPinToJson(InputAction->GetPressedPin()));
            Json->SetObjectField(TEXT("released_pin"), SemanticPinToJson(InputAction->GetReleasedPin()));
        }
        else if (const UK2Node_InputKey* InputKey = Cast<UK2Node_InputKey>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_key"));
            AddKeyToJson(Json, TEXT("input_key"), InputKey->InputKey);
            AddInputBindingFlagsToJson(Json, InputKey->bConsumeInput != 0, InputKey->bExecuteWhenPaused != 0, InputKey->bOverrideParentBinding != 0);
            Json->SetBoolField(TEXT("requires_control"), InputKey->bControl != 0);
            Json->SetBoolField(TEXT("requires_alt"), InputKey->bAlt != 0);
            Json->SetBoolField(TEXT("requires_shift"), InputKey->bShift != 0);
            Json->SetBoolField(TEXT("requires_command"), InputKey->bCommand != 0);
            Json->SetObjectField(TEXT("pressed_pin"), SemanticPinToJson(InputKey->GetPressedPin()));
            Json->SetObjectField(TEXT("released_pin"), SemanticPinToJson(InputKey->GetReleasedPin()));
        }
        else if (const UK2Node_InputTouch* InputTouch = Cast<UK2Node_InputTouch>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_touch"));
            AddInputBindingFlagsToJson(Json, InputTouch->bConsumeInput != 0, InputTouch->bExecuteWhenPaused != 0, InputTouch->bOverrideParentBinding != 0);
            Json->SetObjectField(TEXT("pressed_pin"), SemanticPinToJson(InputTouch->GetPressedPin()));
            Json->SetObjectField(TEXT("released_pin"), SemanticPinToJson(InputTouch->GetReleasedPin()));
            Json->SetObjectField(TEXT("moved_pin"), SemanticPinToJson(InputTouch->GetMovedPin()));
            Json->SetObjectField(TEXT("location_pin"), SemanticPinToJson(InputTouch->GetLocationPin()));
            Json->SetObjectField(TEXT("finger_index_pin"), SemanticPinToJson(InputTouch->GetFingerIndexPin()));
        }
        else if (const UK2Node_InputKeyEvent* InputKeyEvent = Cast<UK2Node_InputKeyEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_key_event"));
            AddKeyToJson(Json, TEXT("input_key"), InputKeyEvent->InputChord.Key);
            Json->SetBoolField(TEXT("requires_control"), InputKeyEvent->InputChord.bCtrl);
            Json->SetBoolField(TEXT("requires_alt"), InputKeyEvent->InputChord.bAlt);
            Json->SetBoolField(TEXT("requires_shift"), InputKeyEvent->InputChord.bShift);
            Json->SetBoolField(TEXT("requires_command"), InputKeyEvent->InputChord.bCmd);
            AddInputEventToJson(Json, InputKeyEvent->InputKeyEvent.GetValue());
            AddInputBindingFlagsToJson(Json, InputKeyEvent->bConsumeInput != 0, InputKeyEvent->bExecuteWhenPaused != 0, InputKeyEvent->bOverrideParentBinding != 0);
        }
        else if (const UK2Node_InputTouchEvent* InputTouchEvent = Cast<UK2Node_InputTouchEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("input_touch_event"));
            AddInputEventToJson(Json, InputTouchEvent->InputKeyEvent.GetValue());
            AddInputBindingFlagsToJson(Json, InputTouchEvent->bConsumeInput != 0, InputTouchEvent->bExecuteWhenPaused != 0, InputTouchEvent->bOverrideParentBinding != 0);
        }
        else if (const UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("custom_event"));
            Json->SetStringField(TEXT("custom_function_name"), CustomEvent->CustomFunctionName.ToString());
        }
        else if (const UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("event"));
            Json->SetObjectField(TEXT("event"), MemberReferenceToJson(Event->EventReference));
        }
        else if (const UK2Node_FunctionResult* FunctionResult = Cast<UK2Node_FunctionResult>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("function_result"));
            Json->SetObjectField(TEXT("function"), MemberReferenceToJson(FunctionResult->FunctionReference));
            if (UFunction* SignatureFunction = FunctionResult->FindSignatureFunction())
            {
                Json->SetStringField(TEXT("resolved_function"), SignatureFunction->GetPathName());
                AddFunctionFlagsToJson(Json, SignatureFunction);
            }
        }
        else if (const UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("function_entry"));
            Json->SetObjectField(TEXT("function"), MemberReferenceToJson(FunctionEntry->FunctionReference));
            const int32 ExtraFlags = FunctionEntry->GetExtraFlags();
            if ((ExtraFlags & FUNC_Private) != 0)
            {
                Json->SetStringField(TEXT("access"), TEXT("private"));
            }
            else if ((ExtraFlags & FUNC_Protected) != 0)
            {
                Json->SetStringField(TEXT("access"), TEXT("protected"));
            }
            else
            {
                Json->SetStringField(TEXT("access"), TEXT("public"));
            }
            Json->SetNumberField(TEXT("local_variable_count"), FunctionEntry->LocalVariables.Num());
            Json->SetArrayField(TEXT("local_variables"), LocalVariablesToJson(FunctionEntry->LocalVariables));
        }
        else if (const UK2Node_MacroInstance* MacroInstance = Cast<UK2Node_MacroInstance>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("macro_instance"));
            if (UEdGraph* MacroGraph = MacroInstance->GetMacroGraph())
            {
                Json->SetStringField(TEXT("macro_graph"), MacroGraph->GetPathName());
            }
            if (UBlueprint* SourceBlueprint = MacroInstance->GetSourceBlueprint())
            {
                Json->SetStringField(TEXT("source_blueprint"), SourceBlueprint->GetPathName());
            }
        }
        else if (const UK2Node_Tunnel* Tunnel = Cast<UK2Node_Tunnel>(Node))
        {
            Json->SetStringField(TEXT("kind"), IsTunnelEntryNode(Tunnel) ? TEXT("tunnel_entry") : TEXT("tunnel_exit"));
        }
        else if (const UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("branch"));
            Json->SetObjectField(TEXT("condition_pin"), SemanticPinToJson(Branch->GetConditionPin()));
            Json->SetObjectField(TEXT("then_pin"), SemanticPinToJson(Branch->GetThenPin()));
            Json->SetObjectField(TEXT("else_pin"), SemanticPinToJson(Branch->GetElsePin()));
        }
        else if (const UK2Node_MultiGate* MultiGate = Cast<UK2Node_MultiGate>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("multi_gate"));
            Json->SetArrayField(TEXT("then_pins"), ExecOutputPinsToJson(MultiGate));
        }
        else if (const UK2Node_DoOnceMultiInput* DoOnceMultiInput = Cast<UK2Node_DoOnceMultiInput>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("do_once_multi_input"));
            Json->SetNumberField(TEXT("additional_input_count"), DoOnceMultiInput->NumAdditionalInputs);
            Json->SetObjectField(TEXT("out_pin"), SemanticPinToJson(DoOnceMultiInput->FindOutPin()));
            Json->SetObjectField(TEXT("self_pin"), SemanticPinToJson(DoOnceMultiInput->FindSelfPin()));
        }
        else if (Cast<UK2Node_ExecutionSequence>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("sequence"));
            Json->SetArrayField(TEXT("then_pins"), ExecOutputPinsToJson(Node));
        }
        else if (const UK2Node_Timeline* Timeline = Cast<UK2Node_Timeline>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("timeline"));
            Json->SetStringField(TEXT("timeline_name"), Timeline->TimelineName.ToString());
            Json->SetStringField(TEXT("timeline_guid"), Timeline->TimelineGuid.ToString());
            Json->SetBoolField(TEXT("auto_play"), Timeline->bAutoPlay != 0);
            Json->SetBoolField(TEXT("loop"), Timeline->bLoop != 0);
            Json->SetBoolField(TEXT("replicated"), Timeline->bReplicated != 0);
            Json->SetBoolField(TEXT("ignore_time_dilation"), Timeline->bIgnoreTimeDilation != 0);
            Json->SetObjectField(TEXT("update_pin"), SemanticPinToJson(Timeline->GetUpdatePin()));
            Json->SetObjectField(TEXT("finished_pin"), SemanticPinToJson(Timeline->GetFinishedPin()));
        }
        else if (Cast<UK2Node_Knot>(Node))
        {
            Json->SetStringField(TEXT("kind"), TEXT("reroute"));
        }
        else
        {
            return nullptr;
        }

        return Json;
    }

    FXPinRef MakePinRef(const UEdGraphPin* Pin, const TMap<const UEdGraphNode*, FString>& NodeIds)
    {
        FXPinRef Ref;
        if (!Pin)
        {
            return Ref;
        }

        const UEdGraphNode* Node = Pin->GetOwningNode();
        if (const FString* NodeId = NodeIds.Find(Node))
        {
            Ref.NodeId = *NodeId;
        }
        Ref.NodeGuid = Node ? Node->NodeGuid.ToString() : FString();
        Ref.PinName = Pin->PinName.ToString();
        Ref.PinId = Pin->PinId.ToString();
        return Ref;
    }

    TSharedPtr<FJsonObject> PinRefToJson(const FXPinRef& Ref)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("node_id"), Ref.NodeId);
        Json->SetStringField(TEXT("node_guid"), Ref.NodeGuid);
        Json->SetStringField(TEXT("pin_name"), Ref.PinName);
        Json->SetStringField(TEXT("pin_id"), Ref.PinId);
        return Json;
    }

    TSharedPtr<FJsonObject> NodeRefToJson(const UEdGraphNode* Node, const TMap<const UEdGraphNode*, FString>& NodeIds)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Node)
        {
            return Json;
        }

        if (const FString* NodeId = NodeIds.Find(Node))
        {
            Json->SetStringField(TEXT("node_id"), *NodeId);
        }
        Json->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
        Json->SetStringField(TEXT("title"), NodeTitle(Node));
        Json->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> BuildEntryNodesJson(
        const UEdGraph* Graph,
        const TMap<const UEdGraphNode*, FString>& NodeIds)
    {
        TArray<TSharedPtr<FJsonValue>> Entries;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (IsEntryNode(Node))
            {
                Entries.Add(MakeShared<FJsonValueObject>(NodeRefToJson(Node, NodeIds)));
            }
        }
        return Entries;
    }

    TArray<TSharedPtr<FJsonValue>> BuildExecChainJson(
        const UEdGraph* Graph,
        const TMap<const UEdGraphNode*, FString>& NodeIds,
        TSet<const UEdGraphNode*>& OutReachableNodes)
    {
        TArray<TSharedPtr<FJsonValue>> Edges;
        TArray<const UEdGraphNode*> Queue;
        TSet<const UEdGraphNode*> QueuedNodes;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (IsEntryNode(Node))
            {
                Queue.Add(Node);
                QueuedNodes.Add(Node);
            }
        }

        int32 Cursor = 0;
        while (Cursor < Queue.Num())
        {
            const UEdGraphNode* Node = Queue[Cursor++];
            if (!Node)
            {
                continue;
            }

            OutReachableNodes.Add(Node);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin))
                {
                    continue;
                }

                for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    const UEdGraphNode* TargetNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
                    if (!TargetNode)
                    {
                        continue;
                    }

                    TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
                    Edge->SetObjectField(TEXT("from_node"), NodeRefToJson(Node, NodeIds));
                    Edge->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
                    Edge->SetStringField(TEXT("from_pin_id"), Pin->PinId.ToString());
                    Edge->SetObjectField(TEXT("to_node"), NodeRefToJson(TargetNode, NodeIds));
                    Edge->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
                    Edge->SetStringField(TEXT("to_pin_id"), LinkedPin->PinId.ToString());
                    Edges.Add(MakeShared<FJsonValueObject>(Edge));

                    if (!QueuedNodes.Contains(TargetNode))
                    {
                        Queue.Add(TargetNode);
                        QueuedNodes.Add(TargetNode);
                    }
                }
            }
        }

        return Edges;
    }

    TArray<TSharedPtr<FJsonValue>> BuildOrphanExecNodesJson(
        const UEdGraph* Graph,
        const TMap<const UEdGraphNode*, FString>& NodeIds,
        const TSet<const UEdGraphNode*>& ReachableNodes)
    {
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (!Node || IsEntryNode(Node) || !HasExecInputPin(Node) || ReachableNodes.Contains(Node))
            {
                continue;
            }
            Nodes.Add(MakeShared<FJsonValueObject>(NodeRefToJson(Node, NodeIds)));
        }
        return Nodes;
    }

    TArray<TSharedPtr<FJsonValue>> BuildUnconnectedExecPinsJson(
        const UEdGraph* Graph,
        const TMap<const UEdGraphNode*, FString>& NodeIds)
    {
        TArray<TSharedPtr<FJsonValue>> Pins;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (!Node)
            {
                continue;
            }

            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || !IsExecPin(Pin) || Pin->bHidden || Pin->LinkedTo.Num() > 0)
                {
                    continue;
                }

                TSharedPtr<FJsonObject> PinJson = MakeShared<FJsonObject>();
                PinJson->SetObjectField(TEXT("node"), NodeRefToJson(Node, NodeIds));
                PinJson->SetObjectField(TEXT("pin"), PinRefToJson(MakePinRef(Pin, NodeIds)));
                PinJson->SetStringField(TEXT("direction"), DirectionToString(Pin->Direction));
                PinJson->SetBoolField(TEXT("node_enabled"), Node->IsNodeEnabled());
                Pins.Add(MakeShared<FJsonValueObject>(PinJson));
            }
        }
        return Pins;
    }

    TArray<TSharedPtr<FJsonValue>> BuildEdgesJson(
        const UEdGraph* Graph,
        const TMap<const UEdGraphNode*, FString>& NodeIds,
        int32& OutExecEdgeCount,
        int32& OutDataEdgeCount)
    {
        OutExecEdgeCount = 0;
        OutDataEdgeCount = 0;

        TArray<TSharedPtr<FJsonValue>> Edges;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (!Node)
            {
                continue;
            }

            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Output)
                {
                    continue;
                }

                const bool bExecEdge = IsExecPin(Pin);
                for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    const UEdGraphNode* TargetNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
                    if (!TargetNode)
                    {
                        continue;
                    }

                    TSharedPtr<FJsonObject> Edge = MakeShared<FJsonObject>();
                    Edge->SetStringField(TEXT("kind"), bExecEdge ? TEXT("exec") : TEXT("data"));
                    Edge->SetStringField(TEXT("pin_type"), PinTypeToString(Pin->PinType));
                    Edge->SetObjectField(TEXT("from_node"), NodeRefToJson(Node, NodeIds));
                    Edge->SetStringField(TEXT("from_pin"), Pin->PinName.ToString());
                    Edge->SetStringField(TEXT("from_pin_id"), Pin->PinId.ToString());
                    Edge->SetObjectField(TEXT("to_node"), NodeRefToJson(TargetNode, NodeIds));
                    Edge->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
                    Edge->SetStringField(TEXT("to_pin_id"), LinkedPin->PinId.ToString());
                    Edges.Add(MakeShared<FJsonValueObject>(Edge));

                    if (bExecEdge)
                    {
                        ++OutExecEdgeCount;
                    }
                    else
                    {
                        ++OutDataEdgeCount;
                    }
                }
            }
        }

        return Edges;
    }

    TSharedPtr<FJsonObject> PinToJson(const UEdGraphPin* Pin, const TMap<const UEdGraphNode*, FString>& NodeIds)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Pin)
        {
            return Json;
        }

        Json->SetStringField(TEXT("id"), Pin->PinId.ToString());
        Json->SetStringField(TEXT("persistent_guid"), Pin->PersistentGuid.ToString());
        Json->SetStringField(TEXT("name"), Pin->PinName.ToString());
        Json->SetStringField(TEXT("direction"), DirectionToString(Pin->Direction));
        Json->SetObjectField(TEXT("type"), PinTypeToJson(Pin->PinType));
        Json->SetBoolField(TEXT("is_exec"), IsExecPin(Pin));
        Json->SetBoolField(TEXT("connected"), Pin->LinkedTo.Num() > 0);
        Json->SetBoolField(TEXT("hidden"), Pin->bHidden);

        const FString DefaultValue = Pin->GetDefaultAsString();
        if (!DefaultValue.IsEmpty())
        {
            Json->SetStringField(TEXT("default"), DefaultValue);
        }
        if (!Pin->AutogeneratedDefaultValue.IsEmpty())
        {
            Json->SetStringField(TEXT("autogenerated_default"), Pin->AutogeneratedDefaultValue);
        }
        if (!Pin->DefaultTextValue.IsEmpty())
        {
            Json->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
        }
        if (Pin->DefaultObject)
        {
            Json->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
        }

        TArray<TSharedPtr<FJsonValue>> Links;
        for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            if (LinkedPin)
            {
                Links.Add(MakeShared<FJsonValueObject>(PinRefToJson(MakePinRef(LinkedPin, NodeIds))));
            }
        }
        Json->SetArrayField(TEXT("linked_to"), Links);
        return Json;
    }

    TSharedPtr<FJsonObject> NodeToJson(const UEdGraphNode* Node, const TMap<const UEdGraphNode*, FString>& NodeIds)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Node)
        {
            return Json;
        }

        const FString* NodeId = NodeIds.Find(Node);
        Json->SetStringField(TEXT("id"), NodeId ? *NodeId : FString());
        Json->SetStringField(TEXT("node_guid"), Node->NodeGuid.ToString());
        Json->SetStringField(TEXT("name"), Node->GetName());
        Json->SetStringField(TEXT("class"), Node->GetClass()->GetName());
        Json->SetStringField(TEXT("title"), NodeTitle(Node));
        Json->SetStringField(TEXT("comment"), Node->NodeComment);
        Json->SetNumberField(TEXT("pos_x"), Node->NodePosX);
        Json->SetNumberField(TEXT("pos_y"), Node->NodePosY);
        Json->SetStringField(TEXT("enabled_state"), LexToString(Node->GetDesiredEnabledState()));
        Json->SetBoolField(TEXT("is_enabled"), Node->IsNodeEnabled());
        Json->SetBoolField(TEXT("user_set_enabled_state"), Node->HasUserSetTheEnabledState());
        if (TSharedPtr<FJsonObject> Semantic = NodeSemanticToJson(Node))
        {
            Json->SetObjectField(TEXT("semantic"), Semantic);
        }

        TArray<TSharedPtr<FJsonValue>> Pins;
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin)
            {
                Pins.Add(MakeShared<FJsonValueObject>(PinToJson(Pin, NodeIds)));
            }
        }
        Json->SetArrayField(TEXT("pins"), Pins);
        return Json;
    }

    void BuildNodeIds(const UEdGraph* Graph, TMap<const UEdGraphNode*, FString>& OutNodeIds)
    {
        OutNodeIds.Reset();
        if (!Graph)
        {
            return;
        }

        TArray<UEdGraphNode*> Nodes = GetSortedNodes(Graph);

        int32 Index = 0;
        for (const UEdGraphNode* Node : Nodes)
        {
            if (Node)
            {
                OutNodeIds.Add(Node, FString::Printf(TEXT("N%d"), Index++));
            }
        }
    }

    TSharedPtr<FJsonObject> GraphToJson(const UBlueprint* Blueprint, const UEdGraph* Graph, int32& InOutNodeCount)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Graph)
        {
            return Json;
        }

        TMap<const UEdGraphNode*, FString> NodeIds;
        BuildNodeIds(Graph, NodeIds);

        Json->SetStringField(TEXT("name"), Graph->GetName());
        Json->SetStringField(TEXT("path"), Graph->GetPathName());
        Json->SetStringField(TEXT("type"), GraphTypeToString(Blueprint, Graph));

        TSet<const UEdGraphNode*> ReachableNodes;
        Json->SetArrayField(TEXT("entry_nodes"), BuildEntryNodesJson(Graph, NodeIds));
        Json->SetArrayField(TEXT("exec_chain"), BuildExecChainJson(Graph, NodeIds, ReachableNodes));
        Json->SetArrayField(TEXT("orphan_exec_nodes"), BuildOrphanExecNodesJson(Graph, NodeIds, ReachableNodes));
        Json->SetArrayField(TEXT("unconnected_exec_pins"), BuildUnconnectedExecPinsJson(Graph, NodeIds));

        int32 ExecEdgeCount = 0;
        int32 DataEdgeCount = 0;
        TArray<TSharedPtr<FJsonValue>> Edges = BuildEdgesJson(Graph, NodeIds, ExecEdgeCount, DataEdgeCount);
        Json->SetNumberField(TEXT("edge_count"), Edges.Num());
        Json->SetNumberField(TEXT("exec_edge_count"), ExecEdgeCount);
        Json->SetNumberField(TEXT("data_edge_count"), DataEdgeCount);
        Json->SetArrayField(TEXT("edges"), Edges);

        TArray<TSharedPtr<FJsonValue>> Nodes;
        TArray<UEdGraphNode*> SortedNodes = GetSortedNodes(Graph);

        for (const UEdGraphNode* Node : SortedNodes)
        {
            if (Node)
            {
                Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Node, NodeIds)));
                ++InOutNodeCount;
            }
        }
        Json->SetNumberField(TEXT("node_count"), Nodes.Num());
        Json->SetArrayField(TEXT("nodes"), Nodes);
        return Json;
    }

    TSharedPtr<FJsonObject> BlueprintVariableToJson(const FBPVariableDescription& Variable)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Variable.VarName.ToString());
        Json->SetStringField(TEXT("guid"), Variable.VarGuid.ToString());
        Json->SetStringField(TEXT("friendly_name"), Variable.FriendlyName);
        Json->SetObjectField(TEXT("type"), PinTypeToJson(Variable.VarType));
        Json->SetStringField(TEXT("category"), Variable.Category.ToString());
        Json->SetStringField(TEXT("property_flags"), FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Variable.PropertyFlags)));
        Json->SetNumberField(TEXT("replication_condition"), static_cast<int32>(Variable.ReplicationCondition.GetValue()));
        if (!Variable.RepNotifyFunc.IsNone())
        {
            Json->SetStringField(TEXT("rep_notify"), Variable.RepNotifyFunc.ToString());
        }
        if (!Variable.DefaultValue.IsEmpty())
        {
            Json->SetStringField(TEXT("default"), Variable.DefaultValue);
        }

        TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
        for (const FBPVariableMetaDataEntry& Entry : Variable.MetaDataArray)
        {
            if (!Entry.DataKey.IsNone())
            {
                Metadata->SetStringField(Entry.DataKey.ToString(), Entry.DataValue);
            }
        }
        Json->SetObjectField(TEXT("metadata"), Metadata);
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> BuildBlueprintVariablesJson(const UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> Variables;
        if (!Blueprint)
        {
            return Variables;
        }

        for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
        {
            Variables.Add(MakeShared<FJsonValueObject>(BlueprintVariableToJson(Variable)));
        }
        return Variables;
    }

    bool ShouldExportComponentProperty(const FProperty* Property)
    {
        if (!Property)
        {
            return false;
        }

        if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated))
        {
            return false;
        }

        return Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_Config);
    }

    FString PropertyFlagsToString(EPropertyFlags Flags)
    {
        TArray<FString> FlagNames;
        if ((Flags & CPF_Edit) != 0)
        {
            FlagNames.Add(TEXT("Edit"));
        }
        if ((Flags & CPF_BlueprintVisible) != 0)
        {
            FlagNames.Add(TEXT("BlueprintVisible"));
        }
        if ((Flags & CPF_BlueprintReadOnly) != 0)
        {
            FlagNames.Add(TEXT("BlueprintReadOnly"));
        }
        if ((Flags & CPF_Config) != 0)
        {
            FlagNames.Add(TEXT("Config"));
        }
        if ((Flags & CPF_DisableEditOnInstance) != 0)
        {
            FlagNames.Add(TEXT("DisableEditOnInstance"));
        }
        if ((Flags & CPF_DisableEditOnTemplate) != 0)
        {
            FlagNames.Add(TEXT("DisableEditOnTemplate"));
        }
        return FString::Join(FlagNames, TEXT("|"));
    }

    TSharedPtr<FJsonObject> ObjectPropertyToJson(const UObject* Object, const FProperty* Property)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Property ? Property->GetName() : FString());
        Json->SetStringField(TEXT("display_name"), Property ? Property->GetDisplayNameText().ToString() : FString());
        Json->SetStringField(TEXT("cpp_type"), Property ? Property->GetCPPType() : FString());
        Json->SetStringField(TEXT("flags"), Property ? PropertyFlagsToString(Property->GetPropertyFlags()) : FString());

        if (!Object || !Property)
        {
            return Json;
        }

        FString ValueText;
        Property->ExportTextItem_Direct(
            ValueText,
            Property->ContainerPtrToValuePtr<void>(Object),
            nullptr,
            const_cast<UObject*>(Object),
            PPF_None);
        Json->SetStringField(TEXT("value"), ValueText);
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> ObjectPropertiesToJson(const UObject* Object)
    {
        TArray<TSharedPtr<FJsonValue>> Properties;
        if (!Object)
        {
            return Properties;
        }

        for (TFieldIterator<FProperty> PropertyIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
        {
            const FProperty* Property = *PropertyIt;
            if (ShouldExportComponentProperty(Property))
            {
                Properties.Add(MakeShared<FJsonValueObject>(ObjectPropertyToJson(Object, Property)));
            }
        }

        return Properties;
    }

    TArray<TSharedPtr<FJsonValue>> ComponentPropertiesToJson(const UActorComponent* Component)
    {
        return ObjectPropertiesToJson(Component);
    }

    FString ComponentCreationMethodToString(EComponentCreationMethod CreationMethod)
    {
        return StaticEnum<EComponentCreationMethod>()
            ? StaticEnum<EComponentCreationMethod>()->GetNameStringByValue(static_cast<int64>(CreationMethod))
            : FString::FromInt(static_cast<int32>(CreationMethod));
    }

    TSharedPtr<FJsonObject> VectorToJson(const FVector& Vector)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Vector.X);
        Json->SetNumberField(TEXT("y"), Vector.Y);
        Json->SetNumberField(TEXT("z"), Vector.Z);
        Json->SetStringField(TEXT("text"), Vector.ToString());
        return Json;
    }

    TSharedPtr<FJsonObject> RotatorToJson(const FRotator& Rotator)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("pitch"), Rotator.Pitch);
        Json->SetNumberField(TEXT("yaw"), Rotator.Yaw);
        Json->SetNumberField(TEXT("roll"), Rotator.Roll);
        Json->SetStringField(TEXT("text"), Rotator.ToString());
        return Json;
    }

    TSharedPtr<FJsonObject> SceneComponentToJson(const USceneComponent* SceneComponent)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!SceneComponent)
        {
            return Json;
        }

        Json->SetObjectField(TEXT("relative_location"), VectorToJson(SceneComponent->GetRelativeLocation()));
        Json->SetObjectField(TEXT("relative_rotation"), RotatorToJson(SceneComponent->GetRelativeRotation()));
        Json->SetObjectField(TEXT("relative_scale"), VectorToJson(SceneComponent->GetRelativeScale3D()));
        Json->SetStringField(TEXT("mobility"), StaticEnum<EComponentMobility::Type>()
            ? StaticEnum<EComponentMobility::Type>()->GetNameStringByValue(static_cast<int64>(SceneComponent->Mobility.GetValue()))
            : FString::FromInt(static_cast<int32>(SceneComponent->Mobility.GetValue())));
        Json->SetStringField(TEXT("attach_socket"), SceneComponent->GetAttachSocketName().ToString());
        Json->SetStringField(TEXT("attach_parent"), SceneComponent->GetAttachParent()
            ? SceneComponent->GetAttachParent()->GetName()
            : FString());
        return Json;
    }

    FString SplinePointTypeToString(ESplinePointType::Type PointType)
    {
        switch (PointType)
        {
        case ESplinePointType::Linear:
            return TEXT("Linear");
        case ESplinePointType::Curve:
            return TEXT("Curve");
        case ESplinePointType::Constant:
            return TEXT("Constant");
        case ESplinePointType::CurveClamped:
            return TEXT("CurveClamped");
        case ESplinePointType::CurveCustomTangent:
            return TEXT("CurveCustomTangent");
        default:
            return FString::FromInt(static_cast<int32>(PointType));
        }
    }

    TSharedPtr<FJsonObject> SplineComponentToJson(const USplineComponent* SplineComponent)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!SplineComponent)
        {
            return Json;
        }

        const int32 PointCount = SplineComponent->GetNumberOfSplinePoints();
        Json->SetNumberField(TEXT("point_count"), PointCount);
        Json->SetNumberField(TEXT("length"), SplineComponent->GetSplineLength());
        Json->SetBoolField(TEXT("closed_loop"), SplineComponent->IsClosedLoop());

        TArray<TSharedPtr<FJsonValue>> Points;
        for (int32 Index = 0; Index < PointCount; ++Index)
        {
            TSharedPtr<FJsonObject> PointJson = MakeShared<FJsonObject>();
            PointJson->SetNumberField(TEXT("index"), Index);
            PointJson->SetStringField(TEXT("type"), SplinePointTypeToString(SplineComponent->GetSplinePointType(Index)));
            PointJson->SetObjectField(TEXT("local_location"), VectorToJson(SplineComponent->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local)));
            PointJson->SetObjectField(TEXT("world_location"), VectorToJson(SplineComponent->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World)));
            PointJson->SetObjectField(TEXT("local_arrive_tangent"), VectorToJson(SplineComponent->GetArriveTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local)));
            PointJson->SetObjectField(TEXT("local_leave_tangent"), VectorToJson(SplineComponent->GetLeaveTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local)));
            PointJson->SetObjectField(TEXT("rotation"), RotatorToJson(SplineComponent->GetRotationAtSplinePoint(Index, ESplineCoordinateSpace::Local)));
            PointJson->SetObjectField(TEXT("scale"), VectorToJson(SplineComponent->GetScaleAtSplinePoint(Index)));
            Points.Add(MakeShared<FJsonValueObject>(PointJson));
        }
        Json->SetArrayField(TEXT("points"), Points);
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> ComponentChildVariableNamesToJson(const USCS_Node* Node)
    {
        TArray<TSharedPtr<FJsonValue>> Children;
        if (!Node)
        {
            return Children;
        }

        for (const USCS_Node* ChildNode : Node->GetChildNodes())
        {
            if (ChildNode)
            {
                Children.Add(MakeShared<FJsonValueString>(ChildNode->GetVariableName().ToString()));
            }
        }

        return Children;
    }

    TSharedPtr<FJsonObject> ComponentTemplateToJson(
        const UActorComponent* ComponentTemplate,
        const FName VariableName,
        const FName ParentVariableName,
        const bool bFromSimpleConstructionScript,
        const USCS_Node* SimpleConstructionScriptNode = nullptr)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!ComponentTemplate)
        {
            return Json;
        }

        Json->SetStringField(TEXT("name"), ComponentTemplate->GetName());
        Json->SetStringField(TEXT("variable_name"), VariableName.ToString());
        Json->SetStringField(TEXT("class"), ComponentTemplate->GetClass() ? ComponentTemplate->GetClass()->GetName() : FString());
        Json->SetStringField(TEXT("class_path"), ComponentTemplate->GetClass() ? ComponentTemplate->GetClass()->GetPathName() : FString());
        Json->SetStringField(TEXT("template_path"), ComponentTemplate->GetPathName());
        Json->SetStringField(TEXT("parent_variable_name"), ParentVariableName.ToString());
        Json->SetBoolField(TEXT("from_simple_construction_script"), bFromSimpleConstructionScript);
        Json->SetBoolField(TEXT("auto_activate"), ComponentTemplate->bAutoActivate);
        Json->SetBoolField(TEXT("editable_when_inherited"), ComponentTemplate->bEditableWhenInherited);
        Json->SetStringField(TEXT("creation_method"), ComponentCreationMethodToString(ComponentTemplate->CreationMethod));
        if (SimpleConstructionScriptNode)
        {
            const UClass* ComponentClass = SimpleConstructionScriptNode->ComponentClass.Get();
            Json->SetStringField(TEXT("scs_node_path"), SimpleConstructionScriptNode->GetPathName());
            Json->SetStringField(TEXT("component_class_path"), ComponentClass ? ComponentClass->GetPathName() : FString());
            Json->SetStringField(TEXT("attach_to_name"), SimpleConstructionScriptNode->AttachToName.ToString());
            Json->SetStringField(TEXT("parent_component_owner_class_name"), SimpleConstructionScriptNode->ParentComponentOwnerClassName.ToString());
            Json->SetBoolField(TEXT("is_parent_component_native"), SimpleConstructionScriptNode->bIsParentComponentNative);
            Json->SetArrayField(TEXT("child_variable_names"), ComponentChildVariableNamesToJson(SimpleConstructionScriptNode));
        }

        if (const USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentTemplate))
        {
            Json->SetObjectField(TEXT("scene"), SceneComponentToJson(SceneComponent));
        }
        if (const USplineComponent* SplineComponent = Cast<USplineComponent>(ComponentTemplate))
        {
            Json->SetObjectField(TEXT("spline"), SplineComponentToJson(SplineComponent));
        }

        TArray<TSharedPtr<FJsonValue>> Properties = ComponentPropertiesToJson(ComponentTemplate);
        Json->SetNumberField(TEXT("property_count"), Properties.Num());
        Json->SetArrayField(TEXT("properties"), Properties);
        return Json;
    }

    TSharedPtr<FJsonObject> ClassDefaultsToJson(const UBlueprint* Blueprint)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        const UClass* GeneratedClass = Blueprint ? Blueprint->GeneratedClass : nullptr;
        const UObject* DefaultObject = GeneratedClass ? GeneratedClass->GetDefaultObject(false) : nullptr;
        if (!DefaultObject)
        {
            return Json;
        }

        Json->SetStringField(TEXT("class"), GeneratedClass->GetName());
        Json->SetStringField(TEXT("class_path"), GeneratedClass->GetPathName());
        Json->SetStringField(TEXT("default_object_path"), DefaultObject->GetPathName());
        TArray<TSharedPtr<FJsonValue>> Properties = ObjectPropertiesToJson(DefaultObject);
        Json->SetNumberField(TEXT("property_count"), Properties.Num());
        Json->SetArrayField(TEXT("properties"), Properties);
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> DefaultObjectComponentsToJson(const UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> Components;
        const UClass* GeneratedClass = Blueprint ? Blueprint->GeneratedClass : nullptr;
        const AActor* ActorDefaultObject = GeneratedClass ? Cast<AActor>(GeneratedClass->GetDefaultObject(false)) : nullptr;
        if (!ActorDefaultObject)
        {
            return Components;
        }

        TArray<UActorComponent*> ActorComponents;
        ActorDefaultObject->GetComponents(ActorComponents);
        for (const UActorComponent* Component : ActorComponents)
        {
            if (!Component)
            {
                continue;
            }

            TSharedPtr<FJsonObject> ComponentJson = ComponentTemplateToJson(Component, Component->GetFName(), NAME_None, false);
            ComponentJson->SetStringField(TEXT("owner_path"), Component->GetOwner() ? Component->GetOwner()->GetPathName() : FString());
            ComponentJson->SetBoolField(TEXT("from_default_object"), true);
            Components.Add(MakeShared<FJsonValueObject>(ComponentJson));
        }

        return Components;
    }

    TSharedPtr<FJsonObject> ComponentTreeNodeToJson(const USCS_Node* Node)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Node)
        {
            return Json;
        }

        const UActorComponent* ComponentTemplate = Node->ComponentTemplate;
        const UClass* ComponentClass = Node->ComponentClass.Get();
        Json->SetStringField(TEXT("node_path"), Node->GetPathName());
        Json->SetStringField(TEXT("variable_name"), Node->GetVariableName().ToString());
        Json->SetStringField(TEXT("component_class"), ComponentClass ? ComponentClass->GetName() : FString());
        Json->SetStringField(TEXT("component_class_path"), ComponentClass ? ComponentClass->GetPathName() : FString());
        Json->SetStringField(TEXT("parent_variable_name"), Node->ParentComponentOrVariableName.ToString());
        Json->SetStringField(TEXT("parent_component_owner_class_name"), Node->ParentComponentOwnerClassName.ToString());
        Json->SetStringField(TEXT("attach_to_name"), Node->AttachToName.ToString());
        Json->SetBoolField(TEXT("is_parent_component_native"), Node->bIsParentComponentNative);
        Json->SetBoolField(TEXT("has_component_template"), ComponentTemplate != nullptr);
        if (ComponentTemplate)
        {
            Json->SetObjectField(TEXT("component"), ComponentTemplateToJson(
                ComponentTemplate,
                Node->GetVariableName(),
                Node->ParentComponentOrVariableName,
                true,
                Node));
        }

        TArray<TSharedPtr<FJsonValue>> Children;
        for (const USCS_Node* ChildNode : Node->GetChildNodes())
        {
            if (ChildNode)
            {
                Children.Add(MakeShared<FJsonValueObject>(ComponentTreeNodeToJson(ChildNode)));
            }
        }
        Json->SetNumberField(TEXT("child_count"), Children.Num());
        Json->SetArrayField(TEXT("children"), Children);
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> BuildBlueprintComponentTreeJson(const UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> ComponentTree;
        if (!Blueprint || !Blueprint->SimpleConstructionScript)
        {
            return ComponentTree;
        }

        for (const USCS_Node* RootNode : Blueprint->SimpleConstructionScript->GetRootNodes())
        {
            if (RootNode)
            {
                ComponentTree.Add(MakeShared<FJsonValueObject>(ComponentTreeNodeToJson(RootNode)));
            }
        }

        return ComponentTree;
    }

    TArray<TSharedPtr<FJsonValue>> BuildBlueprintComponentsJson(const UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> Components;
        if (!Blueprint)
        {
            return Components;
        }

        TSet<const UActorComponent*> ExportedComponents;

        if (const USimpleConstructionScript* SimpleConstructionScript = Blueprint->SimpleConstructionScript)
        {
            for (const USCS_Node* Node : SimpleConstructionScript->GetAllNodes())
            {
                const UActorComponent* ComponentTemplate = Node ? Node->ComponentTemplate : nullptr;
                if (!ComponentTemplate)
                {
                    continue;
                }

                Components.Add(MakeShared<FJsonValueObject>(ComponentTemplateToJson(
                    ComponentTemplate,
                    Node->GetVariableName(),
                    Node->ParentComponentOrVariableName,
                    true,
                    Node)));
                ExportedComponents.Add(ComponentTemplate);
            }
        }

        for (const UActorComponent* ComponentTemplate : Blueprint->ComponentTemplates)
        {
            if (!ComponentTemplate || ExportedComponents.Contains(ComponentTemplate))
            {
                continue;
            }

            Components.Add(MakeShared<FJsonValueObject>(ComponentTemplateToJson(
                ComponentTemplate,
                NAME_None,
                NAME_None,
                false)));
            ExportedComponents.Add(ComponentTemplate);
        }

        return Components;
    }

    void AppendComponentPropertiesMarkdown(
        FString& Markdown,
        const UActorComponent* ComponentTemplate,
        const int32 PropertyLimit)
    {
        if (!ComponentTemplate || PropertyLimit <= 0)
        {
            return;
        }

        int32 ExportedCount = 0;
        for (TFieldIterator<FProperty> PropertyIt(ComponentTemplate->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
        {
            const FProperty* Property = *PropertyIt;
            if (!ShouldExportComponentProperty(Property))
            {
                continue;
            }

            FString ValueText;
            Property->ExportTextItem_Direct(
                ValueText,
                Property->ContainerPtrToValuePtr<void>(ComponentTemplate),
                nullptr,
                const_cast<UActorComponent*>(ComponentTemplate),
                PPF_None);

            Markdown += FString::Printf(
                TEXT("  - `%s` (%s, %s): `%s`\n"),
                *Property->GetName(),
                *Property->GetCPPType(),
                *PropertyFlagsToString(Property->GetPropertyFlags()),
                *ValueText);
            ++ExportedCount;
            if (ExportedCount >= PropertyLimit)
            {
                Markdown += FString::Printf(TEXT("  - ... 属性较多，Markdown 仅显示前 %d 项；JSON 包含完整属性列表\n"), PropertyLimit);
                break;
            }
        }
    }

    void AppendOneComponentMarkdown(
        FString& Markdown,
        const UActorComponent* ComponentTemplate,
        const FName VariableName,
        const FName ParentVariableName,
        const bool bFromSimpleConstructionScript)
    {
        if (!ComponentTemplate)
        {
            return;
        }

        const FString DisplayName = VariableName.IsNone() ? ComponentTemplate->GetName() : VariableName.ToString();
        Markdown += FString::Printf(
            TEXT("### %s\n"),
            *DisplayName);
        Markdown += FString::Printf(
            TEXT("- 类: `%s`\n"),
            ComponentTemplate->GetClass() ? *ComponentTemplate->GetClass()->GetPathName() : TEXT(""));
        Markdown += FString::Printf(
            TEXT("- 模板路径: `%s`\n"),
            *ComponentTemplate->GetPathName());
        Markdown += FString::Printf(
            TEXT("- 来源: `%s`\n"),
            bFromSimpleConstructionScript ? TEXT("SimpleConstructionScript") : TEXT("ComponentTemplates"));
        Markdown += FString::Printf(
            TEXT("- AutoActivate: `%s`, EditableWhenInherited: `%s`\n"),
            ComponentTemplate->bAutoActivate ? TEXT("true") : TEXT("false"),
            ComponentTemplate->bEditableWhenInherited ? TEXT("true") : TEXT("false"));
        if (!ParentVariableName.IsNone())
        {
            Markdown += FString::Printf(TEXT("- 父组件: `%s`\n"), *ParentVariableName.ToString());
        }

        if (const USceneComponent* SceneComponent = Cast<USceneComponent>(ComponentTemplate))
        {
            const FString MobilityText = StaticEnum<EComponentMobility::Type>()
                ? StaticEnum<EComponentMobility::Type>()->GetNameStringByValue(static_cast<int64>(SceneComponent->Mobility.GetValue()))
                : FString::FromInt(static_cast<int32>(SceneComponent->Mobility.GetValue()));
            Markdown += FString::Printf(
                TEXT("- 相对变换: Location `%s`, Rotation `%s`, Scale `%s`\n"),
                *SceneComponent->GetRelativeLocation().ToString(),
                *SceneComponent->GetRelativeRotation().ToString(),
                *SceneComponent->GetRelativeScale3D().ToString());
            Markdown += FString::Printf(
                TEXT("- Mobility: `%s`, AttachParent: `%s`, Socket: `%s`\n"),
                *MobilityText,
                SceneComponent->GetAttachParent() ? *SceneComponent->GetAttachParent()->GetName() : TEXT(""),
                *SceneComponent->GetAttachSocketName().ToString());
        }

        Markdown += TEXT("- 关键参数:\n");
        AppendComponentPropertiesMarkdown(Markdown, ComponentTemplate, 20);
        Markdown += TEXT("\n");
    }

    void AppendComponentTreeNodeMarkdown(FString& Markdown, const USCS_Node* Node, int32 Depth)
    {
        if (!Node)
        {
            return;
        }

        const UActorComponent* ComponentTemplate = Node->ComponentTemplate;
        const UClass* ComponentClass = Node->ComponentClass.Get();
        const FString Indent = FString::ChrN(Depth * 2, TEXT(' '));
        const FString VariableName = Node->GetVariableName().ToString();
        const FString ClassName = ComponentClass
            ? ComponentClass->GetPathName()
            : (ComponentTemplate && ComponentTemplate->GetClass() ? ComponentTemplate->GetClass()->GetPathName() : FString());

        Markdown += FString::Printf(
            TEXT("%s- `%s` : `%s`\n"),
            *Indent,
            *VariableName,
            *ClassName);
        if (!Node->ParentComponentOrVariableName.IsNone())
        {
            Markdown += FString::Printf(
                TEXT("%s  - 父组件: `%s`\n"),
                *Indent,
                *Node->ParentComponentOrVariableName.ToString());
        }
        if (!Node->AttachToName.IsNone())
        {
            Markdown += FString::Printf(
                TEXT("%s  - Socket/Bone: `%s`\n"),
                *Indent,
                *Node->AttachToName.ToString());
        }
        if (!ComponentTemplate)
        {
            Markdown += FString::Printf(TEXT("%s  - 无组件模板\n"), *Indent);
        }

        for (const USCS_Node* ChildNode : Node->GetChildNodes())
        {
            AppendComponentTreeNodeMarkdown(Markdown, ChildNode, Depth + 1);
        }
    }

    void AppendComponentsMarkdown(FString& Markdown, const UBlueprint* Blueprint)
    {
        if (!Blueprint)
        {
            return;
        }

        TSet<const UActorComponent*> ExportedComponents;
        int32 ComponentCount = 0;
        FString ComponentTreeMarkdown;
        FString ComponentMarkdown;

        if (const USimpleConstructionScript* SimpleConstructionScript = Blueprint->SimpleConstructionScript)
        {
            ComponentTreeMarkdown += TEXT("### SCS 树\n\n");
            for (const USCS_Node* RootNode : SimpleConstructionScript->GetRootNodes())
            {
                AppendComponentTreeNodeMarkdown(ComponentTreeMarkdown, RootNode, 0);
            }
            ComponentTreeMarkdown += TEXT("\n");

            for (const USCS_Node* Node : SimpleConstructionScript->GetAllNodes())
            {
                const UActorComponent* ComponentTemplate = Node ? Node->ComponentTemplate : nullptr;
                if (!ComponentTemplate)
                {
                    continue;
                }

                AppendOneComponentMarkdown(
                    ComponentMarkdown,
                    ComponentTemplate,
                    Node->GetVariableName(),
                    Node->ParentComponentOrVariableName,
                    true);
                ExportedComponents.Add(ComponentTemplate);
                ++ComponentCount;
            }
        }

        for (const UActorComponent* ComponentTemplate : Blueprint->ComponentTemplates)
        {
            if (!ComponentTemplate || ExportedComponents.Contains(ComponentTemplate))
            {
                continue;
            }

            AppendOneComponentMarkdown(ComponentMarkdown, ComponentTemplate, NAME_None, NAME_None, false);
            ExportedComponents.Add(ComponentTemplate);
            ++ComponentCount;
        }

        Markdown += TEXT("## 组件树\n\n");
        Markdown += FString::Printf(TEXT("- 组件数: %d\n\n"), ComponentCount);
        Markdown += ComponentTreeMarkdown;
        if (ComponentCount == 0)
        {
            Markdown += TEXT("- 无组件模板信息\n\n");
            return;
        }

        Markdown += TEXT("### 组件详情\n\n");
        Markdown += ComponentMarkdown;
    }

    TSharedPtr<FJsonObject> RichCurveKeyToJson(const FRichCurveKey& Key)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("time"), Key.Time);
        Json->SetNumberField(TEXT("value"), Key.Value);
        Json->SetStringField(TEXT("interp_mode"), RichCurveInterpModeToString(Key.InterpMode));
        Json->SetStringField(TEXT("tangent_mode"), RichCurveTangentModeToString(Key.TangentMode));
        Json->SetStringField(TEXT("tangent_weight_mode"), RichCurveTangentWeightModeToString(Key.TangentWeightMode));
        Json->SetNumberField(TEXT("arrive_tangent"), Key.ArriveTangent);
        Json->SetNumberField(TEXT("leave_tangent"), Key.LeaveTangent);
        Json->SetNumberField(TEXT("arrive_tangent_weight"), Key.ArriveTangentWeight);
        Json->SetNumberField(TEXT("leave_tangent_weight"), Key.LeaveTangentWeight);
        return Json;
    }

    TSharedPtr<FJsonObject> RichCurveToJson(const FRichCurve* Curve)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> Keys;
        if (Curve)
        {
            for (const FRichCurveKey& Key : Curve->GetConstRefOfKeys())
            {
                Keys.Add(MakeShared<FJsonValueObject>(RichCurveKeyToJson(Key)));
            }
        }
        Json->SetNumberField(TEXT("key_count"), Keys.Num());
        Json->SetArrayField(TEXT("keys"), Keys);
        return Json;
    }

    TSharedPtr<FJsonObject> FloatTrackToJson(const FTTFloatTrack& Track)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
        Json->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
        Json->SetBoolField(TEXT("external_curve"), Track.bIsExternalCurve);
        if (const UCurveFloat* Curve = Track.CurveFloat.Get())
        {
            Json->SetStringField(TEXT("curve_path"), Curve->GetPathName());
            Json->SetObjectField(TEXT("curve"), RichCurveToJson(&Curve->FloatCurve));
        }
        else
        {
            Json->SetObjectField(TEXT("curve"), RichCurveToJson(nullptr));
        }
        return Json;
    }

    TSharedPtr<FJsonObject> EventTrackToJson(const FTTEventTrack& Track)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
        Json->SetStringField(TEXT("function_name"), Track.GetFunctionName().ToString());
        Json->SetBoolField(TEXT("external_curve"), Track.bIsExternalCurve);
        if (const UCurveFloat* Curve = Track.CurveKeys.Get())
        {
            Json->SetStringField(TEXT("curve_path"), Curve->GetPathName());
            Json->SetObjectField(TEXT("curve"), RichCurveToJson(&Curve->FloatCurve));
        }
        else
        {
            Json->SetObjectField(TEXT("curve"), RichCurveToJson(nullptr));
        }
        return Json;
    }

    TSharedPtr<FJsonObject> ComponentCurveToJson(const FString& ComponentName, const FRichCurve* Curve)
    {
        TSharedPtr<FJsonObject> Json = RichCurveToJson(Curve);
        Json->SetStringField(TEXT("component"), ComponentName);
        return Json;
    }

    TSharedPtr<FJsonObject> VectorTrackToJson(const FTTVectorTrack& Track)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
        Json->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
        Json->SetBoolField(TEXT("external_curve"), Track.bIsExternalCurve);

        TArray<TSharedPtr<FJsonValue>> Components;
        const UCurveVector* Curve = Track.CurveVector.Get();
        if (Curve)
        {
            Json->SetStringField(TEXT("curve_path"), Curve->GetPathName());
        }

        const TCHAR* ComponentNames[] = { TEXT("x"), TEXT("y"), TEXT("z") };
        for (int32 Index = 0; Index < 3; ++Index)
        {
            Components.Add(MakeShared<FJsonValueObject>(ComponentCurveToJson(ComponentNames[Index], Curve ? &Curve->FloatCurves[Index] : nullptr)));
        }
        Json->SetArrayField(TEXT("components"), Components);
        return Json;
    }

    TSharedPtr<FJsonObject> LinearColorTrackToJson(const FTTLinearColorTrack& Track)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Track.GetTrackName().ToString());
        Json->SetStringField(TEXT("property_name"), Track.GetPropertyName().ToString());
        Json->SetBoolField(TEXT("external_curve"), Track.bIsExternalCurve);

        TArray<TSharedPtr<FJsonValue>> Components;
        const UCurveLinearColor* Curve = Track.CurveLinearColor.Get();
        if (Curve)
        {
            Json->SetStringField(TEXT("curve_path"), Curve->GetPathName());
        }

        const TCHAR* ComponentNames[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Components.Add(MakeShared<FJsonValueObject>(ComponentCurveToJson(ComponentNames[Index], Curve ? &Curve->FloatCurves[Index] : nullptr)));
        }
        Json->SetArrayField(TEXT("components"), Components);
        return Json;
    }

    TSharedPtr<FJsonObject> TimelineToJson(const UTimelineTemplate* Timeline)
    {
        TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
        if (!Timeline)
        {
            return Json;
        }

        Json->SetStringField(TEXT("name"), Timeline->GetName());
        Json->SetStringField(TEXT("variable_name"), Timeline->GetVariableName().ToString());
        Json->SetStringField(TEXT("guid"), Timeline->TimelineGuid.ToString());
        Json->SetNumberField(TEXT("length"), Timeline->TimelineLength);
        Json->SetStringField(TEXT("length_mode"), TimelineLengthModeToString(Timeline->LengthMode));
        Json->SetBoolField(TEXT("auto_play"), Timeline->bAutoPlay != 0);
        Json->SetBoolField(TEXT("loop"), Timeline->bLoop != 0);
        Json->SetBoolField(TEXT("replicated"), Timeline->bReplicated != 0);
        Json->SetBoolField(TEXT("ignore_time_dilation"), Timeline->bIgnoreTimeDilation != 0);
        Json->SetStringField(TEXT("direction_property_name"), Timeline->GetDirectionPropertyName().ToString());
        Json->SetStringField(TEXT("update_function_name"), Timeline->GetUpdateFunctionName().ToString());
        Json->SetStringField(TEXT("finished_function_name"), Timeline->GetFinishedFunctionName().ToString());

        TArray<TSharedPtr<FJsonValue>> FloatTracks;
        for (const FTTFloatTrack& Track : Timeline->FloatTracks)
        {
            FloatTracks.Add(MakeShared<FJsonValueObject>(FloatTrackToJson(Track)));
        }
        Json->SetNumberField(TEXT("float_track_count"), FloatTracks.Num());
        Json->SetArrayField(TEXT("float_tracks"), FloatTracks);

        TArray<TSharedPtr<FJsonValue>> VectorTracks;
        for (const FTTVectorTrack& Track : Timeline->VectorTracks)
        {
            VectorTracks.Add(MakeShared<FJsonValueObject>(VectorTrackToJson(Track)));
        }
        Json->SetNumberField(TEXT("vector_track_count"), VectorTracks.Num());
        Json->SetArrayField(TEXT("vector_tracks"), VectorTracks);

        TArray<TSharedPtr<FJsonValue>> LinearColorTracks;
        for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
        {
            LinearColorTracks.Add(MakeShared<FJsonValueObject>(LinearColorTrackToJson(Track)));
        }
        Json->SetNumberField(TEXT("linear_color_track_count"), LinearColorTracks.Num());
        Json->SetArrayField(TEXT("linear_color_tracks"), LinearColorTracks);

        TArray<TSharedPtr<FJsonValue>> EventTracks;
        for (const FTTEventTrack& Track : Timeline->EventTracks)
        {
            EventTracks.Add(MakeShared<FJsonValueObject>(EventTrackToJson(Track)));
        }
        Json->SetNumberField(TEXT("event_track_count"), EventTracks.Num());
        Json->SetArrayField(TEXT("event_tracks"), EventTracks);

        return Json;
    }

    FString CompactFloat(float Value)
    {
        return FString::SanitizeFloat(Value);
    }

    void AppendRichCurveMarkdown(FString& Markdown, const FRichCurve* Curve, const FString& Prefix)
    {
        if (!Curve)
        {
            Markdown += FString::Printf(TEXT("%s- 无曲线\n"), *Prefix);
            return;
        }

        const TArray<FRichCurveKey>& Keys = Curve->GetConstRefOfKeys();
        if (Keys.IsEmpty())
        {
            Markdown += FString::Printf(TEXT("%s- 无键帧\n"), *Prefix);
            return;
        }

        for (const FRichCurveKey& Key : Keys)
        {
            Markdown += FString::Printf(
                TEXT("%s- t=%s, value=%s, interp=%s, tangent=%s, arrive=%s, leave=%s\n"),
                *Prefix,
                *CompactFloat(Key.Time),
                *CompactFloat(Key.Value),
                *RichCurveInterpModeToString(Key.InterpMode),
                *RichCurveTangentModeToString(Key.TangentMode),
                *CompactFloat(Key.ArriveTangent),
                *CompactFloat(Key.LeaveTangent));
        }
    }

    void AppendTimelinesMarkdown(FString& Markdown, const UBlueprint* Blueprint)
    {
        if (!Blueprint || Blueprint->Timelines.IsEmpty())
        {
            return;
        }

        Markdown += TEXT("## Timeline曲线\n\n");
        for (const UTimelineTemplate* Timeline : Blueprint->Timelines)
        {
            if (!Timeline)
            {
                continue;
            }

            Markdown += FString::Printf(
                TEXT("### %s\n"),
                *Timeline->GetVariableName().ToString());
            Markdown += FString::Printf(
                TEXT("- 长度: %s, 模式: %s, 自动播放: %s, 循环: %s\n"),
                *CompactFloat(Timeline->TimelineLength),
                *TimelineLengthModeToString(Timeline->LengthMode),
                Timeline->bAutoPlay ? TEXT("true") : TEXT("false"),
                Timeline->bLoop ? TEXT("true") : TEXT("false"));

            for (const FTTFloatTrack& Track : Timeline->FloatTracks)
            {
                Markdown += FString::Printf(TEXT("- Float轨道 `%s`\n"), *Track.GetTrackName().ToString());
                const UCurveFloat* Curve = Track.CurveFloat.Get();
                AppendRichCurveMarkdown(Markdown, Curve ? &Curve->FloatCurve : nullptr, TEXT("  "));
            }

            for (const FTTVectorTrack& Track : Timeline->VectorTracks)
            {
                Markdown += FString::Printf(TEXT("- Vector轨道 `%s`\n"), *Track.GetTrackName().ToString());
                const UCurveVector* Curve = Track.CurveVector.Get();
                const TCHAR* ComponentNames[] = { TEXT("x"), TEXT("y"), TEXT("z") };
                for (int32 Index = 0; Index < 3; ++Index)
                {
                    Markdown += FString::Printf(TEXT("  - %s\n"), ComponentNames[Index]);
                    AppendRichCurveMarkdown(Markdown, Curve ? &Curve->FloatCurves[Index] : nullptr, TEXT("    "));
                }
            }

            for (const FTTLinearColorTrack& Track : Timeline->LinearColorTracks)
            {
                Markdown += FString::Printf(TEXT("- LinearColor轨道 `%s`\n"), *Track.GetTrackName().ToString());
                const UCurveLinearColor* Curve = Track.CurveLinearColor.Get();
                const TCHAR* ComponentNames[] = { TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a") };
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    Markdown += FString::Printf(TEXT("  - %s\n"), ComponentNames[Index]);
                    AppendRichCurveMarkdown(Markdown, Curve ? &Curve->FloatCurves[Index] : nullptr, TEXT("    "));
                }
            }

            for (const FTTEventTrack& Track : Timeline->EventTracks)
            {
                Markdown += FString::Printf(TEXT("- Event轨道 `%s` -> `%s`\n"), *Track.GetTrackName().ToString(), *Track.GetFunctionName().ToString());
                const UCurveFloat* Curve = Track.CurveKeys.Get();
                AppendRichCurveMarkdown(Markdown, Curve ? &Curve->FloatCurve : nullptr, TEXT("  "));
            }

            Markdown += TEXT("\n");
        }
    }

    TArray<TSharedPtr<FJsonValue>> BuildTimelinesJson(const UBlueprint* Blueprint)
    {
        TArray<TSharedPtr<FJsonValue>> Timelines;
        if (!Blueprint)
        {
            return Timelines;
        }

        for (const UTimelineTemplate* Timeline : Blueprint->Timelines)
        {
            if (Timeline)
            {
                Timelines.Add(MakeShared<FJsonValueObject>(TimelineToJson(Timeline)));
            }
        }
        return Timelines;
    }

    TSharedPtr<FJsonObject> BlueprintToJson(UBlueprint* Blueprint)
    {
        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("schema_version"), TEXT("1.0"));
        Root->SetStringField(TEXT("generated_by"), TEXT("XTools Blueprint Graph Exporter"));
        Root->SetStringField(TEXT("asset_name"), Blueprint->GetName());
        Root->SetStringField(TEXT("asset_path"), Blueprint->GetPathName());
        Root->SetStringField(TEXT("blueprint_class"), Blueprint->GetClass()->GetName());
        Root->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
        Root->SetObjectField(TEXT("class_defaults"), ClassDefaultsToJson(Blueprint));

        TArray<TSharedPtr<FJsonValue>> Variables = BuildBlueprintVariablesJson(Blueprint);
        Root->SetNumberField(TEXT("variable_count"), Variables.Num());
        Root->SetArrayField(TEXT("variables"), Variables);

        TArray<TSharedPtr<FJsonValue>> Components = BuildBlueprintComponentsJson(Blueprint);
        Root->SetNumberField(TEXT("component_count"), Components.Num());
        Root->SetArrayField(TEXT("components"), Components);
        Root->SetArrayField(TEXT("component_tree"), BuildBlueprintComponentTreeJson(Blueprint));
        TArray<TSharedPtr<FJsonValue>> DefaultObjectComponents = DefaultObjectComponentsToJson(Blueprint);
        Root->SetNumberField(TEXT("default_object_component_count"), DefaultObjectComponents.Num());
        Root->SetArrayField(TEXT("default_object_components"), DefaultObjectComponents);

        TArray<TSharedPtr<FJsonValue>> Timelines = BuildTimelinesJson(Blueprint);
        Root->SetNumberField(TEXT("timeline_count"), Timelines.Num());
        Root->SetArrayField(TEXT("timelines"), Timelines);

        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);

        int32 NodeCount = 0;
        TArray<TSharedPtr<FJsonValue>> GraphValues;
        for (UEdGraph* Graph : Graphs)
        {
            if (Graph)
            {
                GraphValues.Add(MakeShared<FJsonValueObject>(GraphToJson(Blueprint, Graph, NodeCount)));
            }
        }

        Root->SetNumberField(TEXT("graph_count"), GraphValues.Num());
        Root->SetNumberField(TEXT("node_count"), NodeCount);
        Root->SetArrayField(TEXT("graphs"), GraphValues);
        return Root;
    }

    void AppendExecChainMarkdown(
        FString& Markdown,
        const UEdGraph* Graph,
        const TMap<const UEdGraphNode*, FString>& NodeIds,
        TSet<const UEdGraphNode*>& OutReachableNodes)
    {
        TArray<const UEdGraphNode*> Entries;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (IsEntryNode(Node))
            {
                Entries.Add(Node);
            }
        }

        if (Entries.Num() == 0)
        {
            Markdown += TEXT("- 未发现入口节点，无法生成入口可达执行链\n");
            return;
        }

        for (const UEdGraphNode* EntryNode : Entries)
        {
            Markdown += FString::Printf(TEXT("#### %s\n"), *NodeLabel(EntryNode, NodeIds));

            TArray<const UEdGraphNode*> Queue;
            TSet<const UEdGraphNode*> QueuedNodes;
            Queue.Add(EntryNode);
            QueuedNodes.Add(EntryNode);

            int32 ExecEdgeCount = 0;
            int32 Cursor = 0;
            while (Cursor < Queue.Num())
            {
                const UEdGraphNode* Node = Queue[Cursor++];
                if (!Node)
                {
                    continue;
                }

                OutReachableNodes.Add(Node);
                for (const UEdGraphPin* Pin : Node->Pins)
                {
                    if (!Pin || Pin->Direction != EGPD_Output || !IsExecPin(Pin))
                    {
                        continue;
                    }

                    for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
                    {
                        const UEdGraphNode* TargetNode = LinkedPin ? LinkedPin->GetOwningNode() : nullptr;
                        if (!TargetNode)
                        {
                            continue;
                        }

                        if (!IsKnotNode(Node))
                        {
                            for (const UEdGraphPin* DisplayPin : ResolveDisplayPinsAfterKnot(LinkedPin))
                            {
                                const UEdGraphNode* DisplayTargetNode = DisplayPin ? DisplayPin->GetOwningNode() : nullptr;
                                if (!DisplayTargetNode)
                                {
                                    continue;
                                }

                                Markdown += FString::Printf(
                                    TEXT("- %s.`%s` -> %s.`%s`\n"),
                                    *NodeLabel(Node, NodeIds),
                                    *Pin->PinName.ToString(),
                                    *NodeLabel(DisplayTargetNode, NodeIds),
                                    *DisplayPin->PinName.ToString());
                                ++ExecEdgeCount;
                            }
                        }

                        if (!QueuedNodes.Contains(TargetNode))
                        {
                            Queue.Add(TargetNode);
                            QueuedNodes.Add(TargetNode);
                        }
                    }
                }
            }

            if (ExecEdgeCount == 0)
            {
                Markdown += TEXT("- 无后续执行线\n");
            }
        }
    }

    void AppendUnreachableNodesMarkdown(
        FString& Markdown,
        const UEdGraph* Graph,
        const TMap<const UEdGraphNode*, FString>& NodeIds,
        const TSet<const UEdGraphNode*>& ReachableNodes)
    {
        int32 Count = 0;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (!Node || IsEntryNode(Node) || !HasExecInputPin(Node) || ReachableNodes.Contains(Node))
            {
                continue;
            }

            Markdown += FString::Printf(TEXT("- %s\n"), *NodeLabel(Node, NodeIds));
            ++Count;
        }

        if (Count == 0)
        {
            Markdown += TEXT("- 无\n");
        }
    }

    void AppendGraphMarkdown(FString& Markdown, UBlueprint* Blueprint, UEdGraph* Graph)
    {
        if (!Graph)
        {
            return;
        }

        TMap<const UEdGraphNode*, FString> NodeIds;
        BuildNodeIds(Graph, NodeIds);

        Markdown += FString::Printf(TEXT("## %s (%s)\n\n"), *Graph->GetName(), *GraphTypeToString(Blueprint, Graph));
        Markdown += FString::Printf(TEXT("- 节点数: %d\n\n"), NodeIds.Num());

        Markdown += TEXT("### 入口节点\n");
        int32 EntryCount = 0;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (IsEntryNode(Node))
            {
                Markdown += FString::Printf(TEXT("- %s\n"), *NodeLabel(Node, NodeIds));
                ++EntryCount;
            }
        }
        if (EntryCount == 0)
        {
            Markdown += TEXT("- 未发现入口节点\n");
        }
        Markdown += TEXT("\n");

        Markdown += TEXT("### 入口可达执行流\n");
        TSet<const UEdGraphNode*> ReachableNodes;
        AppendExecChainMarkdown(Markdown, Graph, NodeIds, ReachableNodes);
        Markdown += TEXT("\n");

        Markdown += TEXT("### 孤立执行节点\n");
        AppendUnreachableNodesMarkdown(Markdown, Graph, NodeIds, ReachableNodes);
        Markdown += TEXT("\n");

        Markdown += TEXT("### 数据连接摘要\n");
        int32 DataEdgeCount = 0;
        const int32 DataEdgeLimit = 100;
        for (const UEdGraphNode* Node : GetSortedNodes(Graph))
        {
            if (!Node || IsKnotNode(Node) || DataEdgeCount >= DataEdgeLimit)
            {
                continue;
            }
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (DataEdgeCount >= DataEdgeLimit)
                {
                    break;
                }
                if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin))
                {
                    continue;
                }
                for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    for (const UEdGraphPin* DisplayPin : ResolveDisplayPinsAfterKnot(LinkedPin))
                    {
                        const UEdGraphNode* TargetNode = DisplayPin ? DisplayPin->GetOwningNode() : nullptr;
                        if (!TargetNode)
                        {
                            continue;
                        }
                        Markdown += FString::Printf(
                            TEXT("- %s.`%s` -> %s.`%s`\n"),
                            *NodeLabel(Node, NodeIds),
                            *Pin->PinName.ToString(),
                            *NodeLabel(TargetNode, NodeIds),
                            *DisplayPin->PinName.ToString());
                        ++DataEdgeCount;
                        if (DataEdgeCount >= DataEdgeLimit)
                        {
                            break;
                        }
                    }
                    if (DataEdgeCount >= DataEdgeLimit)
                    {
                        break;
                    }
                }
            }
        }
        if (DataEdgeCount == 0)
        {
            Markdown += TEXT("- 无\n");
        }
        else if (DataEdgeCount >= DataEdgeLimit)
        {
            Markdown += FString::Printf(TEXT("- ... 已截断，仅显示前 %d 条数据连接\n"), DataEdgeLimit);
        }
        Markdown += TEXT("\n");
    }

    FString BlueprintToMarkdown(UBlueprint* Blueprint)
    {
        FString Markdown;
        Markdown += FString::Printf(TEXT("# %s 蓝图逻辑流\n\n"), *Blueprint->GetName());
        Markdown += FString::Printf(TEXT("- 资产路径: `%s`\n"), *Blueprint->GetPathName());
        Markdown += FString::Printf(TEXT("- 父类: `%s`\n\n"), Blueprint->ParentClass ? *Blueprint->ParentClass->GetPathName() : TEXT(""));

        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        int32 ValidGraphCount = 0;
        for (const UEdGraph* Graph : Graphs)
        {
            if (Graph)
            {
                ++ValidGraphCount;
            }
        }
        Markdown += FString::Printf(TEXT("- 图表数: %d\n\n"), ValidGraphCount);

        AppendComponentsMarkdown(Markdown, Blueprint);
        AppendTimelinesMarkdown(Markdown, Blueprint);

        for (UEdGraph* Graph : Graphs)
        {
            AppendGraphMarkdown(Markdown, Blueprint, Graph);
        }
        return Markdown;
    }

    bool SaveJson(const TSharedPtr<FJsonObject>& Json, const FString& FilePath)
    {
        FString JsonText;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
        if (!FJsonSerializer::Serialize(Json.ToSharedRef(), Writer))
        {
            return false;
        }
        return FFileHelper::SaveStringToFile(JsonText, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }

    bool ExportOneBlueprint(UBlueprint* Blueprint, FString& OutOutputDir, FString& OutError)
    {
        if (!Blueprint)
        {
            OutError = TEXT("无效蓝图资产");
            return false;
        }

        const FString PackageDirName = SanitizeFileName(Blueprint->GetOutermost()->GetName().Replace(TEXT("/"), TEXT("_")));
        OutOutputDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("XTools") / TEXT("BlueprintExports") / PackageDirName);
        if (!IFileManager::Get().MakeDirectory(*OutOutputDir, true))
        {
            OutError = FString::Printf(TEXT("%s: 输出目录创建失败"), *Blueprint->GetName());
            return false;
        }

        const FString BaseName = SanitizeFileName(Blueprint->GetName());
        const FString JsonPath = OutOutputDir / (BaseName + TEXT(".json"));
        const FString MarkdownPath = OutOutputDir / (BaseName + TEXT(".md"));

        if (!SaveJson(BlueprintToJson(Blueprint), JsonPath))
        {
            OutError = FString::Printf(TEXT("%s: JSON写入失败"), *Blueprint->GetName());
            return false;
        }

        const FString Markdown = BlueprintToMarkdown(Blueprint);
        if (!FFileHelper::SaveStringToFile(Markdown, *MarkdownPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            OutError = FString::Printf(TEXT("%s: Markdown写入失败"), *Blueprint->GetName());
            return false;
        }

        return true;
    }

    void ShowExportResultDialog(const FString& Message, const FString& OutputRoot)
    {
        const bool bHasOutputRoot = !OutputRoot.IsEmpty();

        TSharedRef<SWindow> Window = SNew(SWindow)
            .Title(LOCTEXT("BlueprintGraphExportResultTitle", "蓝图逻辑流导出"))
            .SizingRule(ESizingRule::Autosized)
            .SupportsMaximize(false)
            .SupportsMinimize(false);

        const TWeakPtr<SWindow> WeakWindow = Window;

        Window->SetContent(
            SNew(SBox)
            .WidthOverride(620.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(16.0f, 14.0f, 16.0f, 8.0f)
                [
                    SNew(SBox)
                    .MaxDesiredHeight(320.0f)
                    [
                        SNew(SScrollBox)

                        + SScrollBox::Slot()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(Message))
                            .AutoWrapText(true)
                        ]
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(16.0f, 0.0f, 16.0f, 16.0f)
                .HAlign(HAlign_Right)
                [
                    SNew(SUniformGridPanel)
                    .SlotPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))

                    + SUniformGridPanel::Slot(0, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("OpenBlueprintGraphExportFolder", "打开目录"))
                        .IsEnabled(bHasOutputRoot)
                        .OnClicked_Lambda([OutputRoot]()
                        {
                            FPlatformProcess::ExploreFolder(*OutputRoot);
                            return FReply::Handled();
                        })
                    ]

                    + SUniformGridPanel::Slot(1, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("CopyBlueprintGraphExportPath", "复制绝对路径"))
                        .IsEnabled(bHasOutputRoot)
                        .OnClicked_Lambda([OutputRoot]()
                        {
                            FPlatformApplicationMisc::ClipboardCopy(*OutputRoot);
                            FNotificationInfo NotificationInfo(LOCTEXT("BlueprintGraphExportPathCopied", "已复制导出路径"));
                            NotificationInfo.ExpireDuration = 2.0f;
                            FSlateNotificationManager::Get().AddNotification(NotificationInfo);
                            return FReply::Handled();
                        })
                    ]

                    + SUniformGridPanel::Slot(2, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("CloseBlueprintGraphExportResult", "关闭"))
                        .OnClicked_Lambda([WeakWindow]()
                        {
                            if (const TSharedPtr<SWindow> PinnedWindow = WeakWindow.Pin())
                            {
                                PinnedWindow->RequestDestroyWindow();
                            }
                            return FReply::Handled();
                        })
                    ]
                ]
            ]);

        FSlateApplication::Get().AddModalWindow(Window, FSlateApplication::Get().GetActiveTopLevelWindow());
    }
}

void FX_BlueprintGraphExporter::ExportBlueprints(const TArray<FAssetData>& SelectedAssets)
{
    TArray<FAssetData> BlueprintAssets;
    for (const FAssetData& AssetData : SelectedAssets)
    {
        if (IsBlueprintAssetData(AssetData))
        {
            BlueprintAssets.Add(AssetData);
        }
    }

    if (BlueprintAssets.Num() == 0)
    {
        FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoBlueprintSelected", "未选中可导出的蓝图资产。"));
        return;
    }

    int32 SuccessCount = 0;
    int32 FailedCount = 0;
    bool bCanceled = false;
    const FString OutputRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("XTools") / TEXT("BlueprintExports"));
    TArray<FString> Errors;
    TArray<FString> SuccessfulOutputDirs;

    {
        FScopedSlowTask SlowTask(BlueprintAssets.Num(), LOCTEXT("ExportBlueprintGraphSlowTask", "正在导出蓝图逻辑流..."));
        SlowTask.MakeDialog(true);

        for (const FAssetData& AssetData : BlueprintAssets)
        {
            if (SlowTask.ShouldCancel())
            {
                bCanceled = true;
                break;
            }

            SlowTask.EnterProgressFrame(1.0f, FText::Format(
                LOCTEXT("ExportBlueprintGraphProgress", "正在导出 {0}"),
                FText::FromName(AssetData.AssetName)));

            UBlueprint* Blueprint = Cast<UBlueprint>(AssetData.GetAsset());
            if (!Blueprint)
            {
                ++FailedCount;
                const FString Error = FString::Printf(TEXT("%s: 蓝图资产加载失败"), *AssetData.GetObjectPathString());
                Errors.Add(Error);
                UE_LOG(LogX_AssetEditor, Warning, TEXT("蓝图逻辑流导出失败: %s"), *Error);
                continue;
            }

            FString OutputDir;
            FString Error;
            if (ExportOneBlueprint(Blueprint, OutputDir, Error))
            {
                ++SuccessCount;
                SuccessfulOutputDirs.AddUnique(OutputDir);
                UE_LOG(LogX_AssetEditor, Log, TEXT("已导出蓝图逻辑流: %s"), *OutputDir);
            }
            else
            {
                ++FailedCount;
                Errors.Add(Error);
                UE_LOG(LogX_AssetEditor, Warning, TEXT("蓝图逻辑流导出失败: %s"), *Error);
            }
        }
    }

    FString Message = FString::Printf(TEXT("蓝图逻辑流导出完成：成功 %d，失败 %d。"), SuccessCount, FailedCount);
    if (bCanceled)
    {
        Message += TEXT("\n\n操作已取消。");
    }
    if (SuccessCount > 0)
    {
        const FString DisplayOutputDir = SuccessfulOutputDirs.Num() == 1 ? SuccessfulOutputDirs[0] : OutputRoot;
        Message += FString::Printf(TEXT("\n\n输出目录:\n%s"), *DisplayOutputDir);
    }
    if (Errors.Num() > 0)
    {
        Message += TEXT("\n\n错误:\n");
        Message += FString::Join(Errors, TEXT("\n"));
    }
    const FString ActionOutputDir = SuccessfulOutputDirs.Num() == 1 ? SuccessfulOutputDirs[0] : OutputRoot;
    ShowExportResultDialog(Message, SuccessCount > 0 ? ActionOutputDir : FString());
}

#undef LOCTEXT_NAMESPACE
