/*
* Copyright (c) 2025 XIYBHK
* Licensed under UE_XTools License
*/

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "XToolsErrorReporter.h"
#include "BlueprintExtensions.h"

class FKismetCompilerContext;

/**
 * K2Node 安全验证帮助类
 *
 * 提供统一的节点验证和错误处理机制，防止编译时崩溃
 *
 * 设计原则：
 * - 温和失败：验证失败时记录错误但不触发 check/ensure
 * - 统一日志：使用 XToolsErrorReporter 统一错误报告
 * - 清理资源：失败时调用 BreakAllNodeLinks 清理状态
 * - 上下文信息：错误消息包含节点路径便于调试
 */
struct FK2NodeSafetyHelpers
{
	/**
	 * 验证引脚是否存在且有效
	 * @param Pin 要验证的引脚
	 * @param PinName 引脚名称（用于错误消息）
	 * @param Node 所属节点
	 * @param CompilerContext 编译器上下文
	 * @param ErrorMessage 自定义错误消息（可选）
	 * @return 引脚有效返回 true，否则记录错误并返回 false
	 */
	static bool ValidatePin(
		const UEdGraphPin* Pin,
		const FName& PinName,
		UK2Node* Node,
		FKismetCompilerContext& CompilerContext,
		const FText& ErrorMessage = FText::GetEmpty()
	);

	/**
	 * 验证引脚是否已连接
	 * @param Pin 要验证的引脚
	 * @param PinName 引脚名称（用于错误消息）
	 * @param Node 所属节点
	 * @param CompilerContext 编译器上下文
	 * @param bAllowEmpty 是否允许未连接（false 时未连接会报错）
	 * @return 引脚已连接或允许空连接返回 true，否则记录错误并返回 false
	 */
	static bool ValidatePinConnection(
		const UEdGraphPin* Pin,
		const FName& PinName,
		UK2Node* Node,
		FKismetCompilerContext& CompilerContext,
		bool bAllowEmpty = false
	);

	/**
	 * 验证中间节点是否成功创建
	 * @param IntermediateNode 中间节点指针
	 * @param NodeTypeName 节点类型名称（用于错误消息）
	 * @param OwnerNode 所属节点
	 * @param CompilerContext 编译器上下文
	 * @return 节点有效返回 true，否则记录错误并返回 false
	 */
	static bool ValidateIntermediateNode(
		const UEdGraphNode* IntermediateNode,
		const FString& NodeTypeName,
		UK2Node* OwnerNode,
		FKismetCompilerContext& CompilerContext
	);

	/**
	 * 验证图表 Schema 是否有效
	 * @param Schema Schema 指针
	 * @param Node 所属节点
	 * @param CompilerContext 编译器上下文
	 * @return Schema 有效返回 true，否则记录错误并返回 false
	 */
	static bool ValidateSchema(
		const UEdGraphSchema_K2* Schema,
		UK2Node* Node,
		FKismetCompilerContext& CompilerContext
	);

	/**
	 * 安全地重建节点引脚
	 * @param Node 要重建的节点
	 * @return 重建成功返回 true，失败返回 false
	 */
	static bool SafeReconstructNode(UK2Node* Node);

	/**
	 * 验证并传播通配符引脚类型
	 * @param SourcePin 源引脚（类型来源）
	 * @param TargetPin 目标引脚（类型目标）
	 * @param Node 所属节点
	 * @param bNotifyGraphChanged 是否通知图表变更
	 * @return 类型传播成功返回 true
	 */
	static bool PropagateWildcardPinType(
		UEdGraphPin* SourcePin,
		UEdGraphPin* TargetPin,
		UK2Node* Node,
		bool bNotifyGraphChanged = true
	);

private:
	/** 内部辅助函数：格式化错误消息 */
	static FString FormatErrorMessage(
		const FString& BaseMessage,
		const UK2Node* Node
	);

	/** 内部辅助函数：记录编译错误 */
	static void LogCompileError(
		const FString& Message,
		UK2Node* Node,
		FKismetCompilerContext& CompilerContext
	);
};
