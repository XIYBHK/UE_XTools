// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistMisc/BAGraphSchema.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistUtils.h"
#include "Editor.h"
#include "K2Node_Knot.h"
#include "MaterialGraphNode_Knot.h"
#include "NiagaraGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BlueprintAssistMisc/BAControlRigUtils.h"
#include "BlueprintAssistMisc/BAGraphSchemaTypes.h"
#include "EdGraph/RigVMEdGraph.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMControllerActions.h"
#include "SoundCueGraph/SoundCueGraph.h"

const UBAGraphSchema& UBAGraphSchema::Get(UEdGraph* Graph)
{
	if (FBAUtils::IsBlueprintGraph(Graph))
	{
		return *GetDefault<UBAGraphSchema_Blueprint>();
	}

	if (Graph->IsA(UMaterialGraph::StaticClass()))
	{
		return *GetDefault<UBAGraphSchema_Material>();
	}

	if (Graph->IsA(URigVMEdGraph::StaticClass()))
	{
		return *GetDefault<UBAGraphSchema_ControlRig>();
	}

	if (Graph->IsA(USoundCueGraph::StaticClass()))
	{
		return *GetDefault<UBAGraphSchema_SoundCue>();
	}

	if (Graph->IsA(UNiagaraGraph::StaticClass()))
	{
		return *GetDefault<UBAGraphSchema_Niagara>();
	}

	return *GetDefault<UBAGraphSchema>();
}

const UBAGraphSchema& UBAGraphSchema::Get(UEdGraphNode* Node)
{
	UEdGraph* Graph = Node ? Node->GetGraph() : nullptr;
	return Get(Graph);
}

const UBAGraphSchema& UBAGraphSchema::Get(UEdGraphPin* Pin)
{
	UEdGraphNode* Node = Pin ? Pin->GetOwningNode() : nullptr;
	return Get(Node);
}

const UBAGraphSchema& UBAGraphSchema::Get(TSharedPtr<FBAGraphHandler> GraphHandler)
{
	UEdGraph* Graph = GraphHandler ? GraphHandler->GetFocusedEdGraph() : nullptr;
	return Get(Graph);
}

bool UBAGraphSchema::IsExecPin(const UEdGraphPin* Pin) const
{
	return FBAUtils::IsExecPin(Pin);
}

FPinConnectionResponse UBAGraphSchema::GetConnectionResponse(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	if (PinA == nullptr || PinB == nullptr)
	{
		FPinConnectionResponse Response;
		Response.SetFatal();
		return Response;
	}

	const UEdGraphSchema* Schema = PinA->GetOwningNodeUnchecked()->GetGraph()->GetSchema();
	return Schema->CanCreateConnection(PinA, PinB);
}

bool UBAGraphSchema::CanConnectPins(UEdGraphPin* PinA, UEdGraphPin* PinB, bool bOverrideLinks, bool bAcceptConversions, bool bAcceptHiddenPins) const
{
	if (PinA == nullptr || PinB == nullptr)
	{
		return false;
	}

	const UEdGraphSchema* Schema = PinA->GetOwningNodeUnchecked()->GetGraph()->GetSchema();
	ECanCreateConnectionResponse Response = Schema->CanCreateConnection(PinA, PinB).Response;

	if (!bAcceptHiddenPins && (PinA->bHidden || PinB->bHidden))
	{
		return false;
	}

	switch (Response)
	{
	case CONNECT_RESPONSE_MAKE:
		return true;
	case CONNECT_RESPONSE_DISALLOW:
		return false;
	case CONNECT_RESPONSE_BREAK_OTHERS_A:
		return bOverrideLinks;
	case CONNECT_RESPONSE_BREAK_OTHERS_B:
		return bOverrideLinks;
	case CONNECT_RESPONSE_BREAK_OTHERS_AB:
		return bOverrideLinks;
	case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
		return bAcceptConversions;
	case CONNECT_RESPONSE_MAX:
		return false;
	default:
		return false;
	}
}

bool UBAGraphSchema::TryCreateConnection(FBANodePinHandle& PinA, FBANodePinHandle& PinB, EBABreakMethod BreakMethod, bool bConversionAllowed, bool bTryHidden) const
{
	if (!PinA.IsValid() || !PinB.IsValid())
	{
		return false;
	}

	if (!bTryHidden && PinB->bHidden)
	{
		return false;
	}

	const UEdGraphSchema* Schema = FBAUtils::GetSchema(PinA.GetPin());
	check(Schema);
	const FPinConnectionResponse Response = Schema->CanCreateConnection(PinA.GetPin(), PinB.GetPin());
	bool bModified = false;

	TArray<UEdGraphPin*> PreviouslyLinked = PinA->LinkedTo;

	ECanCreateConnectionResponse NewResponse = Response.Response;

	switch (BreakMethod)
	{
		case EBABreakMethod::Always:
		{
			if (Response.Response == CONNECT_RESPONSE_MAKE ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_A ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_B)
			{
				NewResponse = CONNECT_RESPONSE_BREAK_OTHERS_AB;
			}

			break;
		}
		case EBABreakMethod::Never:
		{
			if (Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_AB ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_A ||
				Response.Response == CONNECT_RESPONSE_BREAK_OTHERS_B)
			{
				return false;
			}

			break;
		}
		case EBABreakMethod::Default: // default means that we just follow what the response tells us
		default: ;
	}

	switch (NewResponse)
	{
		case CONNECT_RESPONSE_MAKE:
		{
			FBAUtils::SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}

		case CONNECT_RESPONSE_BREAK_OTHERS_A:
		{
			BreakAllPinLinks(PinA, false);
			FBAUtils::SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}

		case CONNECT_RESPONSE_BREAK_OTHERS_B:
		{
			BreakAllPinLinks(PinB, false);
			FBAUtils::SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}

		case CONNECT_RESPONSE_BREAK_OTHERS_AB:
		{
			BreakAllPinLinks(PinA, false);
			BreakAllPinLinks(PinB, false);
			FBAUtils::SchemaTryCreateConnection_Internal(PinA, PinB);
			bModified = true;
			break;
		}
		case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
			if (bConversionAllowed)
			{
				bModified = Schema->CreateAutomaticConversionNodeAndConnections(PinA.GetPin(), PinB.GetPin());
			}
			break;
		case CONNECT_RESPONSE_DISALLOW:
			break;
		default:
			break;
	}

#if WITH_EDITOR
	if (bModified)
	{
		if (UEdGraphNode* NodeA = PinA.GetNode())
		{
			NodeA->PinConnectionListChanged(PinA.GetPin());
			NodeA->NodeConnectionListChanged();
		}

		if (UEdGraphNode* NodeB = PinB.GetNode())
		{
			NodeB->PinConnectionListChanged(PinB.GetPin());
			NodeB->NodeConnectionListChanged();
		}
	}
#endif

	return bModified;
}

void UBAGraphSchema::BreakSinglePinLink(FBANodePinHandle& A, FBANodePinHandle& B, bool bModify) const
{
	if (A.IsValid() && B.IsValid())
	{
		if (const UEdGraphSchema* Schema = FBAUtils::GetSchema(A.GetPin()))
		{
			if (bModify)
			{
				A->Modify();
				B->Modify();
			}

			Schema->BreakSinglePinLink(A.GetPin(), B.GetPin());
			PostGraphChanged(FBAUtils::GetGraph(A.GetPin()));
		}
	}
}

void UBAGraphSchema::BreakSinglePinLink(FPinLink& Link) const
{
	BreakSinglePinLink(Link.FromNodePinHandle, Link.ToNodePinHandle);
}

void UBAGraphSchema::BreakAllPinLinks(FBANodePinHandle& PinHandle, bool bSendsNotification, bool bModify) const
{
	if (UEdGraphPin* Pin = PinHandle.GetPin())
	{
		if (const UEdGraphSchema* Schema = FBAUtils::GetSchema(Pin))
		{
			if (bModify)
			{
				Pin->Modify();
			}

			Schema->BreakPinLinks(*Pin, bSendsNotification);
			PostGraphChanged(FBAUtils::GetGraph(Pin));
		}
	}
}

void UBAGraphSchema::DeleteNode(UEdGraphNode* Node) const
{
	if (!Node)
	{
		return;
	}

	if (UEdGraph* EdGraph = Node->GetGraph())
	{
		if (const UEdGraphSchema* Schema = EdGraph->GetSchema())
		{
			Schema->SafeDeleteNodeFromGraph(EdGraph, Node);
		}
	}
}

void UBAGraphSchema::SetNodePosition(UEdGraphNode* Node, int32 X, int32 Y, TSharedPtr<SGraphPanel> GraphPanel) const
{
	if (!Node)
	{
		return;
	}

	Node->Modify();

	auto NewPos = FBAVector2(X, Y);

	// GraphNode::MoveTo is the more important function to call
	// only a few graph types implement the Schema::SetNodePosition and many instead rely on MoveTo
	// see: SGraphNode_BehaviorTree, SGraphNodeMaterialBase
	if (TSharedPtr<SGraphNode> GraphNode = FBAUtils::GetGraphNode(GraphPanel, Node))
	{
		TSet<TWeakPtr<SNodePanel::SNode>> NodeSet;
		GraphNode->MoveTo(NewPos, NodeSet);
	}

#if BA_UE_VERSION_OR_LATER(5, 0)
	Node->GetSchema()->SetNodePosition(Node, NewPos);
#else
	Node->NodePosX = NewPos.X;
	Node->NodePosY = NewPos.Y;
#endif
}

bool UBAGraphSchema::SetPinDefaultValue(UEdGraphPin* Pin, const FString& NewDefault, FString* OutError) const
{
	if (!Pin)
	{
		if (OutError)
		{
			*OutError = "Null pin";
		}

		return false;
	}

	if (Pin->bDefaultValueIsIgnored || Pin->bDefaultValueIsReadOnly)
	{
		return false;
	}


	FString NewDefaultValue;
	UObject* NewDefaultObject = nullptr;
	FText NewDefaultTextValue;

	const auto& PC = Pin->PinType.PinCategory;
	if (FBAUtils::IsObjectPinType(PC))
	{
		// Avoid hitting assert if you try to load object from a very long string
		if (NewDefault.Len() >= NAME_SIZE)
		{
			return false;
		}

		NewDefaultObject = NewDefault.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *NewDefault, nullptr, 0, nullptr);
	}
	else if (PC == UEdGraphSchema_K2::PC_Text)
	{
		FText DefaultAsText;
		FTextStringHelper::ReadFromBuffer(*NewDefault, DefaultAsText);
		NewDefaultTextValue = DefaultAsText;
	}
	else if (PC == UEdGraphSchema_K2::PC_SoftObject)
	{
		// SPropertyEditorAsset::OnCopy
		NewDefaultValue = FPackageName::ExportTextPathToObjectPath(NewDefault);
	}
	else if (PC == UEdGraphSchema_K2::PC_Struct)
	{
		if (const UScriptStruct* Struct = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject))
		{
			// if the struct has it's own import text, it *should* already be in the correct format
			if (Struct->StructFlags & STRUCT_ImportTextItemNative)
			{
				NewDefaultValue = NewDefault;
			}
			else if (Struct == TBaseStructure<FLinearColor>::Get())
			{
				NewDefaultValue = NewDefault;
			}
			else
			{
				// currently format: "(P1=V1,P2=V2,P3=V3)"
				// convert it to format: "V1,V2,V3"
				FString NoBrackets = NewDefault;
				if (NoBrackets.RemoveFromStart("(") && NoBrackets.RemoveFromEnd(")"))
				{
					TArray<FString> Properties;
					if (Struct)
					{
						for (TFieldIterator<BA_PROPERTY> PropertyIter(Struct); PropertyIter; ++PropertyIter)
						{
							// skip transient properties
							if (PropertyIter->HasAnyPropertyFlags(CPF_Transient))
							{
								continue;
							}

								Properties.Add(PropertyIter->GetName());
							}
						}

						TArray<FString> InProperties;
						NoBrackets.ParseIntoArray(InProperties, TEXT(","));

						FString ParsedStructValue;
						for (int i = 0; i < InProperties.Num(); ++i)
						{
							FString& Str = InProperties[i];
							FString StructName;
							FString StructValue;
							Str.Split(TEXT("="), &StructName, &StructValue);

							if (!Properties.IsValidIndex(i))
							{
								return false;
							}

							if (!Properties.IsValidIndex(i) || Properties[i] != StructName)
							{
								return false;
							}

							ParsedStructValue.Append(StructValue + ",");
						}

					ParsedStructValue.RemoveFromEnd(",");
					NewDefaultValue = ParsedStructValue;
				}
				else
				{
					return false;
				}
			}
		}
	}
	else
	{
		NewDefaultValue = NewDefault;
	}

	return FBAUtils::TrySetDefaultPinValues(Pin, NewDefaultValue, NewDefaultObject, NewDefaultTextValue, OutError);
}

FString UBAGraphSchema::GetPinDefaultValue(UEdGraphPin* Pin) const
{
	if (!Pin)
	{
		return FString();
	}

	FString DefaultValue = !Pin->DefaultValue.IsEmpty() ? Pin->DefaultValue : Pin->AutogeneratedDefaultValue;

	const auto& PC = Pin->PinType.PinCategory;

	if (FBAUtils::IsObjectPinType(PC))
	{
		return GetPathNameSafe(Pin->DefaultObject);
	}

	if (PC == UEdGraphSchema_K2::PC_Text)
	{
		FString TextAsString;
		FTextStringHelper::WriteToBuffer(TextAsString, Pin->DefaultTextValue);
		return TextAsString;
	}

	if (PC == UEdGraphSchema_K2::PC_SoftObject)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
#if BA_UE_VERSION_OR_LATER(5, 0)
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(Pin->DefaultValue));
#else
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FName(Pin->DefaultValue));
#endif
		return AssetData.GetExportTextName();
	}

	if (PC == UEdGraphSchema_K2::PC_Struct)
	{
		if (const UScriptStruct* StructType = Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject))
		{
			// if the struct has it's own import text, it *should* already be in the correct format
			if (StructType->StructFlags & STRUCT_ImportTextItemNative)
			{
				return DefaultValue;
			}

			// see UEdGraphSchema_K2::SplitPin / UEdGraphSchema_K2::RecombinePin
			// often the pins have no default value at all, so it's easier to init the data type
			// then convert it back to a string to get an accurate default value
			if (StructType == TBaseStructure<FVector>::Get())
			{
				FVector V3;
				V3.InitFromString(FBAUtils::AttachPropertyNamesToValue(DefaultValue, StructType));
				DefaultValue = V3.ToString();
				DefaultValue.ReplaceInline(TEXT(" "), TEXT(","));
			}
			else if (StructType == TBaseStructure<FVector2D>::Get())
			{
				FVector2D V2D;
				V2D.InitFromString(FBAUtils::AttachPropertyNamesToValue(DefaultValue, StructType));
				DefaultValue = V2D.ToString();
				DefaultValue.ReplaceInline(TEXT(" "), TEXT(","));
			}
			else if (StructType == TBaseStructure<FRotator>::Get())
			{
				FRotator Rot;
				DefaultValue = FBAUtils::AttachPropertyNamesToValue(DefaultValue, StructType);
				DefaultValue.ReplaceInline(TEXT("Pitch="), TEXT("P="));
				DefaultValue.ReplaceInline(TEXT("Yaw="), TEXT("Y="));
				DefaultValue.ReplaceInline(TEXT("Roll="), TEXT("R="));
				Rot.InitFromString(DefaultValue);
				DefaultValue = Rot.ToString();
				DefaultValue.ReplaceInline(TEXT(" "), TEXT(","));
				DefaultValue.ReplaceInline(TEXT("P="), TEXT("Pitch="));
				DefaultValue.ReplaceInline(TEXT("Y="), TEXT("Yaw="));
				DefaultValue.ReplaceInline(TEXT("R="), TEXT("Roll="));
			}
			else if (StructType == TBaseStructure<FLinearColor>::Get())
			{
				FLinearColor LC;
				LC.InitFromString(DefaultValue);
				return LC.ToString();
			}
			else
			{
				// for which structs does this actually happen?
				DefaultValue = FBAUtils::AttachPropertyNamesToValue(DefaultValue, StructType);
			}

			return FString::Printf(TEXT("(%s)"), *DefaultValue);
		}
	}

	static TSet<FName> NumTypes = {
		UEdGraphSchema_K2::PC_Int,
		UEdGraphSchema_K2::PC_Int64,
		UEdGraphSchema_K2::PC_Float,
#if BA_UE_VERSION_OR_LATER(5, 0)
		UEdGraphSchema_K2::PC_Double,
		UEdGraphSchema_K2::PC_Real,
#endif
		UEdGraphSchema_K2::PC_Name
	};

	if (NumTypes.Contains(PC))
	{
		if (DefaultValue.IsEmpty())
		{
			return "0";
		}
	}

	return DefaultValue;
}

bool UBAGraphSchema::MakeLinkDirect(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	if (PinA && PinB)
	{
		PinA->MakeLinkTo(PinB);
		return true;
	}

	return false;
}

bool UBAGraphSchema::BreakLinkDirect(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	if (PinA && PinB)
	{
		PinA->BreakLinkTo(PinB);
		return true;
	}

	return false;
}

UEdGraphNode* UBAGraphSchema::MakeKnotNode(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	return FBAUtils::CreateKnotNode(PinA->GetOwningNode()->GetGraph(), GraphPosition, PinA, PinB);
}

FGuid UBAGraphSchema::GetNodeGuid(UEdGraphNode* Node) const
{
	return Node ? Node->NodeGuid : FGuid();
}

FBAScopedGraphAction::FBAScopedGraphAction(TSharedPtr<FBAGraphHandler> GraphHandler, const FString& Transaction)
	: FBAScopedGraphAction(GraphHandler->GetFocusedEdGraph(), Transaction)
{

}

FBAScopedGraphAction::FBAScopedGraphAction(UEdGraph* InGraph, const FString& Transaction)
{
	Graph = InGraph;
	Schema = &UBAGraphSchema::Get(InGraph);

	if (Schema.IsValid() && Graph.IsValid())
	{
		Schema->PreGraphChanged(InGraph);
	}

	if (!Transaction.IsEmpty())
	{
		SetTransaction(Transaction);
	}
}

void FBAScopedGraphAction::SetTransaction(const FString& Transaction)
{
	if (URigVMController* Controller = FBAControlRigUtils::GetVMController(GetGraph()))
	{
		UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("Opened undo bracket! %s"), *Transaction);
		bHasControlRigTransaction = Controller->OpenUndoBracket(Transaction);
	}
	else
	{
		TransactionPtr = MakeShared<FScopedTransaction>(FText::FromString(Transaction));
	}
}

void FBAScopedGraphAction::Cancel()
{
	if (URigVMController* Controller = FBAControlRigUtils::GetVMController(GetGraph()))
	{
		UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("Cancel undo bracket!"));
		Controller->CancelUndoBracket();
		bHasControlRigTransaction = false;
	}
	else
	{
		if (TransactionPtr)
		{
			TransactionPtr->Cancel();
		}

		TransactionPtr.Reset();
	}
}

bool FBAScopedGraphAction::IsOutstanding()
{
	if (TransactionPtr.IsValid())
	{
		return TransactionPtr->IsOutstanding();
	}

	return bHasControlRigTransaction;
}

FBAScopedGraphAction::~FBAScopedGraphAction()
{
	if (bDirty && Schema.IsValid() && Graph.IsValid())
	{
		Schema->PostGraphChanged(GetGraph());
	}

	if (bHasControlRigTransaction)
	{
		if (URigVMController* Controller = FBAControlRigUtils::GetVMController(GetGraph()))
		{
			UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("Closed undo bracket!"));
			Controller->CloseUndoBracket();
		}
	}

	if (TransactionPtr.IsValid())
	{
		TransactionPtr.Reset();
	}
}

TPair<UEdGraphPin*, UEdGraphPin*> FBASchemaUtils::GetKnotNodePins(UEdGraphNode* Node)
{
	if (Node)
	{
		if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Node))
		{
			return {Knot->GetInputPin(), Knot->GetOutputPin()};
		}

		if (UMaterialGraphNode_Knot* Knot = Cast<UMaterialGraphNode_Knot>(Node))
		{
			return {Knot->GetInputPin(), Knot->GetOutputPin()};
		}
	}

	return {};
}
