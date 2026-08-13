#pragma once

#include "CoreMinimal.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_SplineMoveAlong.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;

/** 带中断输入的沿样条线移动异步节点 */
UCLASS()
class SPLINEMOVEMENTEDITOR_API UK2Node_SplineMoveAlong : public UK2Node_BaseAsyncTask
{
	GENERATED_BODY()

public:
	UK2Node_SplineMoveAlong(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetKeywords() const override;

private:
	UEdGraphPin* GetInterruptPin() const;
	UEdGraphPin* GetProxyPin() const;
};
