// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "BAGraphSchema.generated.h"

class SGraphPanel;
class FBAGraphHandler;
class FScopedTransaction;
struct FBANodePinHandle;
struct FPinLink;
enum class EBABreakMethod : uint8;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
struct FPinConnectionResponse;

UCLASS()
class UBAGraphSchema : public UObject
{
	GENERATED_BODY()

public:
	UBAGraphSchema() {}

	virtual ~UBAGraphSchema() override {}

	static const UBAGraphSchema& Get(UEdGraph* Graph);
	static const UBAGraphSchema& Get(UEdGraphNode* Node);
	static const UBAGraphSchema& Get(UEdGraphPin* Pin);
	static const UBAGraphSchema& Get(TSharedPtr<FBAGraphHandler> GraphHandler);

	virtual bool IsExecPin(const UEdGraphPin* Pin) const;

	virtual FPinConnectionResponse GetConnectionResponse(UEdGraphPin* PinA, UEdGraphPin* PinB) const;

	virtual bool CanConnectPins(UEdGraphPin* PinA,
		UEdGraphPin* PinB,
		bool bOverrideLinks = false,
		bool bAcceptConversions = false,
		bool bAcceptHiddenPins = false) const;

	virtual bool TryCreateConnection(FBANodePinHandle& PinA, FBANodePinHandle& PinB, EBABreakMethod BreakMethod, bool bConversionAllowed = false, bool bTryHidden = false) const;

	virtual void BreakSinglePinLink(FBANodePinHandle& PinA, FBANodePinHandle& PinB, bool bModify = true) const;
	void BreakSinglePinLink(FPinLink& Link) const;

	virtual void BreakAllPinLinks(FBANodePinHandle& PinHandle, bool bSendsNotification = true, bool bModify = true) const;
	virtual void DeleteNode(UEdGraphNode* Node) const;

	virtual void SetNodePosition(UEdGraphNode* Node, int32 X, int32 Y, TSharedPtr<SGraphPanel> GraphPanel = {}) const;

	virtual bool SetPinDefaultValue(UEdGraphPin* Pin, const FString& NewDefault, FString* OutError = nullptr) const;
	virtual FString GetPinDefaultValue(UEdGraphPin* Pin) const;

	virtual bool MakeLinkDirect(UEdGraphPin* PinA, UEdGraphPin* PinB) const;
	virtual bool BreakLinkDirect(UEdGraphPin* PinA, UEdGraphPin* PinB) const;
	virtual UEdGraphNode* MakeKnotNode(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const;

	virtual FGuid GetNodeGuid(UEdGraphNode* Node) const;

	virtual void PreGraphChanged(UEdGraph* Graph) const {}
	virtual void PostGraphChanged(UEdGraph* Graph) const {}
	virtual void PostFormatting(UEdGraph* Graph) const {}
};

struct FBAScopedGraphAction : public TSharedFromThis<FBAScopedGraphAction>
{
	bool bDirty = false;
	bool bHasControlRigTransaction = false;
	TSharedPtr<FScopedTransaction> TransactionPtr;

	const UBAGraphSchema* GetSchema() const { return Schema.Get(); }
	UEdGraph* GetGraph() const { return Graph.Get(); }

	explicit FBAScopedGraphAction(TSharedPtr<FBAGraphHandler> GraphHandler, const FString& Transaction = "");
	explicit FBAScopedGraphAction(UEdGraph* InGraph, const FString& Transaction = "");
	void SetTransaction(const FString& Transaction);
	void Cancel();
	bool IsOutstanding();
	~FBAScopedGraphAction();

private:
	TWeakObjectPtr<const UBAGraphSchema> Schema = nullptr;
	TWeakObjectPtr<UEdGraph> Graph = nullptr;
};

struct FBASchemaUtils
{
	static TPair<UEdGraphPin*, UEdGraphPin*> GetKnotNodePins(UEdGraphNode* Node);
};
