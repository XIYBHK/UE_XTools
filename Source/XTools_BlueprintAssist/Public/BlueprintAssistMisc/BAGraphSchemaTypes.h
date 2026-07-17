// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BAGraphSchema.h"
#include "BAGraphSchemaTypes.generated.h"

class URigVMController;
class URigVMPin;

UCLASS()
class UBAGraphSchema_Blueprint : public UBAGraphSchema
{
	GENERATED_BODY()
public:
};

UCLASS()
class UBAGraphSchema_Material : public UBAGraphSchema
{
	GENERATED_BODY()
public:

	virtual FGuid GetNodeGuid(UEdGraphNode* Node) const override;
};

UCLASS()
class UBAGraphSchema_Niagara : public UBAGraphSchema
{
	GENERATED_BODY()
public:
	virtual FString GetPinDefaultValue(UEdGraphPin* Pin) const override;
};


UCLASS()
class UBAGraphSchema_ControlRig : public UBAGraphSchema
{
	GENERATED_BODY()
public:
	virtual bool CanConnectPins(UEdGraphPin* PinA, UEdGraphPin* PinB, bool bOverrideLinks = false, bool bAcceptConversions = false, bool bAcceptHiddenPins = false) const override;
	virtual bool TryCreateConnection(FBANodePinHandle& HandleA, FBANodePinHandle& HandleB, EBABreakMethod BreakMethod, bool bConversionAllowed = false, bool bTryHidden = false) const override;
	virtual void SetNodePosition(UEdGraphNode* Node, int32 X, int32 Y, TSharedPtr<SGraphPanel> GraphPanel = {}) const override;
	virtual bool SetPinDefaultValue(UEdGraphPin* Pin, const FString& NewDefault, FString* OutError = nullptr) const override;
	virtual FString GetPinDefaultValue(UEdGraphPin* Pin) const override;
	virtual FGuid GetNodeGuid(UEdGraphNode* Node) const override;
};

UCLASS()
class UBAGraphSchema_SoundCue : public UBAGraphSchema
{
	GENERATED_BODY()
public:
	virtual void PostGraphChanged(UEdGraph* Graph) const override;
};