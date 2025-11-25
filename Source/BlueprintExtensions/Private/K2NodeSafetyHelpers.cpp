/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#include "K2NodeSafetyHelpers.h"
#include "EdGraphSchema_K2.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "XTools_K2NodeSafetyHelpers"

bool FK2NodeSafetyHelpers::ValidatePin(
	const UEdGraphPin* Pin,
	const FName& PinName,
	UK2Node* Node,
	FKismetCompilerContext& CompilerContext,
	const FText& ErrorMessage)
{
	if (!Pin)
	{
		const FString Message = ErrorMessage.IsEmpty()
			? FString::Printf(TEXT("Pin '%s' not found in node '%s'"), *PinName.ToString(), *Node->GetPathName())
			: ErrorMessage.ToString();

		LogCompileError(Message, Node, CompilerContext);
		return false;
	}

	return true;
}

bool FK2NodeSafetyHelpers::ValidatePinConnection(
	const UEdGraphPin* Pin,
	const FName& PinName,
	UK2Node* Node,
	FKismetCompilerContext& CompilerContext,
	bool bAllowEmpty)
{
	if (!ValidatePin(Pin, PinName, Node, CompilerContext))
	{
		return false;
	}

	if (!bAllowEmpty && Pin->LinkedTo.Num() == 0)
	{
		const FString Message = FString::Printf(
			TEXT("Pin '%s' must be connected in node '%s'"),
			*PinName.ToString(),
			*Node->GetPathName()
		);

		// 温和处理：使用 Warning 而非 Error
		FXToolsErrorReporter::Warning(
			LogBlueprintExtensions,
			Message,
			NAME_None,
			false
		);
		CompilerContext.MessageLog.Warning(*Message, Node);
		return false;
	}

	return true;
}

bool FK2NodeSafetyHelpers::ValidateIntermediateNode(
	const UEdGraphNode* IntermediateNode,
	const FString& NodeTypeName,
	UK2Node* OwnerNode,
	FKismetCompilerContext& CompilerContext)
{
	if (!IntermediateNode)
	{
		const FString Message = FString::Printf(
			TEXT("Failed to spawn intermediate node '%s' in node '%s'"),
			*NodeTypeName,
			*OwnerNode->GetPathName()
		);

		LogCompileError(Message, OwnerNode, CompilerContext);
		return false;
	}

	return true;
}

bool FK2NodeSafetyHelpers::ValidateSchema(
	const UEdGraphSchema_K2* Schema,
	UK2Node* Node,
	FKismetCompilerContext& CompilerContext)
{
	if (!Schema)
	{
		const FString Message = FString::Printf(
			TEXT("Invalid graph schema in node '%s'"),
			*Node->GetPathName()
		);

		LogCompileError(Message, Node, CompilerContext);
		return false;
	}

	return true;
}

bool FK2NodeSafetyHelpers::SafeReconstructNode(UK2Node* Node)
{
	if (!Node)
	{
		FXToolsErrorReporter::Error(
			LogBlueprintExtensions,
			TEXT("SafeReconstructNode: Node is null"),
			NAME_None,
			false
		);
		return false;
	}

	// 备份现有引脚信息
	TArray<UEdGraphPin*> OldPins = Node->Pins;

	// 尝试重建
	Node->ReconstructNode();

	// 验证结果
	bool bSuccess = true;
	for (UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin && OldPin->LinkedTo.Num() > 0)
		{
			// 检查旧连接是否仍然有效
			UEdGraphPin* NewPin = Node->FindPin(OldPin->PinName, OldPin->Direction);
			if (!NewPin || NewPin->LinkedTo.Num() == 0)
			{
				// 连接丢失
				FXToolsErrorReporter::Warning(
					LogBlueprintExtensions,
					FString::Printf(
						TEXT("SafeReconstructNode: Pin connection lost for '%s' in node '%s'"),
						*OldPin->PinName.ToString(),
						*Node->GetPathName()
					),
					NAME_None,
					false
				);
				bSuccess = false;
			}
		}
	}

	return bSuccess;
}

bool FK2NodeSafetyHelpers::PropagateWildcardPinType(
	UEdGraphPin* SourcePin,
	UEdGraphPin* TargetPin,
	UK2Node* Node,
	bool bNotifyGraphChanged)
{
	if (!SourcePin || !TargetPin)
	{
		FXToolsErrorReporter::Error(
			LogBlueprintExtensions,
			FString::Printf(
				TEXT("PropagateWildcardPinType: Invalid pin in node '%s'"),
				Node ? *Node->GetPathName() : TEXT("Unknown")
			),
			NAME_None,
			false
		);
		return false;
	}

	// 源引脚不是通配符且有确定类型
	if (SourcePin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	{
		TargetPin->PinType = SourcePin->PinType;

		if (bNotifyGraphChanged && Node && Node->GetGraph())
		{
			Node->GetGraph()->NotifyGraphChanged();
		}

		return true;
	}

	// 源引脚是通配符，尝试从连接推断
	if (SourcePin->LinkedTo.Num() > 0)
	{
		UEdGraphPin* LinkedPin = SourcePin->LinkedTo[0];
		if (LinkedPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
		{
			TargetPin->PinType = LinkedPin->PinType;

			if (bNotifyGraphChanged && Node && Node->GetGraph())
			{
				Node->GetGraph()->NotifyGraphChanged();
			}

			return true;
		}
	}

	// 无法确定类型
	return false;
}

FString FK2NodeSafetyHelpers::FormatErrorMessage(
	const FString& BaseMessage,
	const UK2Node* Node)
{
	if (!Node)
	{
		return BaseMessage;
	}

	return FString::Printf(TEXT("%s [Node: %s]"), *BaseMessage, *Node->GetPathName());
}

void FK2NodeSafetyHelpers::LogCompileError(
	const FString& Message,
	UK2Node* Node,
	FKismetCompilerContext& CompilerContext)
{
	// 统一错误报告
	FXToolsErrorReporter::Error(
		LogBlueprintExtensions,
		Message,
		NAME_None,
		false  // 编译错误不显示屏幕提示
	);

	// 【修复】编译器消息使用 Warning 而非 Error
	// Error 会触发 EdGraphNode.h:563 断言崩溃
	CompilerContext.MessageLog.Warning(*Message, Node);

	// 清理节点状态
	if (Node)
	{
		Node->BreakAllNodeLinks();
	}
}

#undef LOCTEXT_NAMESPACE
