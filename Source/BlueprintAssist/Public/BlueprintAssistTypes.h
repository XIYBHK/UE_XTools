// Copyright fpwong. All Rights Reserved.

#pragma once

#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistUtils.h"
#include "ScopedTransaction.h"
#include "SGraphPin.h"
#include "BlueprintAssistTypes.generated.h"

#if ENGINE_MINOR_VERSION >= 25 || ENGINE_MAJOR_VERSION >= 5
#define BA_PROPERTY FProperty
#define BA_FIND_FIELD FindUField
#define BA_FIND_PROPERTY FindFProperty
#define BA_WEAK_FIELD_PTR TWeakFieldPtr
#else
#define BA_PROPERTY UProperty
#define BA_FIND_FIELD FindField
#define BA_FIND_PROPERTY FindField
#define BA_WEAK_FIELD_PTR TWeakObjectPtr
#endif

struct FBAGraphPinHandle
{
	TWeakObjectPtr<UEdGraph> Graph = nullptr;
	FGuid NodeGuid;
	FGuid PinId;

	// for when guid fails
	FEdGraphPinType PinType;
	FName PinName;

	FBAGraphPinHandle()
	{
		Invalidate();
	}

	FBAGraphPinHandle(UEdGraphPin* Pin)
	{
		SetPin(Pin);
	}

	void SetPin(UEdGraphPin* Pin);

	UEdGraphPin* GetPin(bool bFallbackOnPinName = true);

	bool IsValid() const
	{
		return Graph != nullptr && PinId.IsValid() && NodeGuid.IsValid();
	}

	void Invalidate()
	{
		Graph = nullptr;
		NodeGuid.Invalidate();
		PinId.Invalidate();
		PinType.ResetToDefaults();
		PinName = NAME_None;
	}

	bool operator==(const FBAGraphPinHandle& Other) const
	{
		return PinId == Other.PinId && NodeGuid == Other.NodeGuid;
	}

	bool operator!=(const FBAGraphPinHandle& Other) const
	{
		return !(*this == Other);
	}

	friend inline uint32 GetTypeHash(const FBAGraphPinHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.PinId), GetTypeHash(Handle.NodeGuid));
	}

	FString ToString() const;
};

// Consider using FEdGraphPinReference
struct FBANodePinHandle
{
	TWeakObjectPtr<UEdGraphNode> Node = nullptr;
	FGuid PinId;

	// for when guid fails
	FEdGraphPinType PinType;
	FName PinName;

	FBANodePinHandle()
	{
		SetPin(nullptr);
	}

	FBANodePinHandle(UEdGraphPin* Pin)
	{
		SetPin(Pin);
	}

	void SetPin(UEdGraphPin* Pin);

	UEdGraphPin* GetPin();

	UEdGraphNode* GetNode() const
	{
		return Node.IsValid() ? Node.Get() : nullptr;
	}

	bool IsValid()
	{
		return Node != nullptr && PinId.IsValid();
	}

	static TArray<FBANodePinHandle> ConvertArray(const TArray<UEdGraphPin*>& Pins)
	{
		TArray<FBANodePinHandle> Handles;

		for (UEdGraphPin* const Pin : Pins)
		{
			Handles.Add(FBANodePinHandle(Pin));
		}

		return Handles;
	}

	bool operator==(const FBANodePinHandle& Other) const
	{
		return PinId == Other.PinId && Node == Other.Node;
	}

	bool operator==(const UEdGraphPin* Other)
	{
		return GetPin() == Other;
	}

	UEdGraphPin* operator->() { return GetPin(); }

	friend inline uint32 GetTypeHash(const FBANodePinHandle& Handle)
	{
		return HashCombine(GetTypeHash(Handle.PinId), GetTypeHash(Handle.Node->NodeGuid));
	}
};

UENUM()
enum class EBADragMethod : uint8
{
	LMB UMETA(DisplayName = "LMB"),
	AdditionalDragChord UMETA(DisplayName = "AdditionalDragChord"),
};

struct FBANodeMovementTransaction
{
	EBADragMethod DragMethod;
	TSharedPtr<FScopedTransaction> TransactionPtr = nullptr;

	bool IsValid() const
	{
		return TransactionPtr.IsValid() && TransactionPtr->IsOutstanding();
	}

	void Begin(const TSet<UEdGraphNode*>& Nodes, const FText& InSessionName, const EBADragMethod& InDragMethod);
	void End(const EBADragMethod& InDragMethod);
};

struct FBAPinDefaultValue
{
	FString PinValue;
	FText PinTextValue;
	TWeakObjectPtr<UObject> PinObject;
};

struct FBANodeArray
{
	void SetArray(const TArray<UEdGraphNode*>& Nodes);
	TArray<TWeakObjectPtr<UEdGraphNode>>& GetNodesWeak() { return NodeArrayWeak; }
	const TArray<TWeakObjectPtr<UEdGraphNode>>& GetNodesWeakConst() const { return NodeArrayWeak; }
	TArray<UEdGraphNode*> GetNodes() const;
	const TArray<UEdGraphNode*>& GetCachedNodes() const { return CachedNodes; }
	void Empty();
	void CacheNodes();

private:
	TArray<TWeakObjectPtr<UEdGraphNode>> NodeArrayWeak;
	TArray<UEdGraphNode*> CachedNodes;
};