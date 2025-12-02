#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PointSamplingTypes.h"

#if WITH_EDITOR
#include "K2Node.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "K2Node_PointSampling.generated.h"

class FBlueprintActionDatabaseRegistrar;
class UEdGraphPin;
class UK2Node_CallFunction;
class FKismetCompilerContext;

/**
 * @brief 智能点采样节点
 * 根据选择的采样模式动态显示相应的参数引脚
 *
 * 支持的采样模式：
 * - 实心矩形、空心矩形、螺旋矩形
 * - 实心三角形、空心三角形
 * - 圆形
 * - 雪花形、雪花弧形
 * - 样条线
 * - 静态模型顶点
 * - 骨骼插槽
 * - 图片像素
 *
 * 通用功能：
 * - 噪波扰动（JitterStrength）
 * - 结果缓存（UseCache）
 * - 坐标空间切换（世界/局部/原始）
 * - 随机种子控制
 */
UCLASS()
class POINTSAMPLING_API UK2Node_PointSampling : public UK2Node
{
	GENERATED_BODY()

public:
	UK2Node_PointSampling(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ UEdGraphNode interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PostReconstructNode() override;
	virtual void EarlyValidation(class FCompilerResultsLog& MessageLog) const override;
	//~ End UEdGraphNode interface

	//~ UK2Node interface
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FName GetCornerIcon() const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual bool IsNodeSafeToIgnore() const override { return true; }
	virtual void PostPasteNode() override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	//~ End UK2Node interface

	/** 获取采样模式引脚 */
	FORCEINLINE UEdGraphPin* GetSamplingModePin() const;

	/** 获取输出位置数组引脚 */
	FORCEINLINE UEdGraphPin* GetOutputPositionsPin() const;

private:
	/**
	 * @brief 根据当前选择的采样模式，重新构建节点的动态输入引脚
	 */
	void RebuildDynamicPins();

	/**
	 * @brief 获取当前选择的采样模式
	 */
	EPointSamplingMode GetCurrentSamplingMode() const;

	// ExpandNode 辅助函数
	bool DetermineSamplingFunction(EPointSamplingMode SamplingMode, FName& OutFunctionName) const;
	bool CheckForErrors(const FKismetCompilerContext& CompilerContext) const;
	void ConnectCommonPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode, EPointSamplingMode SamplingMode);
	void ConnectModeSpecificPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode, EPointSamplingMode SamplingMode);
	void ConnectOutputPins(FKismetCompilerContext& CompilerContext, UK2Node_CallFunction* CallFunctionNode);
	bool MovePinLinksOrCopyDefaults(FKismetCompilerContext& CompilerContext, UEdGraphPin* SourcePin, UEdGraphPin* DestPin);

	// 类型名称映射辅助函数
	FString GetModeDisplayName(EPointSamplingMode SamplingMode) const;
	
	// 引脚查找辅助函数
	UEdGraphPin* FindPinCheckedSafe(const FName& PinName, EEdGraphPinDirection Direction = EGPD_MAX) const;

#if WITH_EDITORONLY_DATA
	/** 防止递归调用的标志 */
	bool bIsReconstructingPins;
#endif
};

#endif // WITH_EDITOR
