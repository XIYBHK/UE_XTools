#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistNodeActions.h"

class FUICommandList;

class XTOOLS_BLUEPRINTASSIST_API FBAPinActionsBase : public FBANodeActionsBase
{
public:
	bool HasSelectedPin() const;
	bool HasEditablePin() const;
	bool HasHoveredPin() const;
	bool HasHoveredOrSelectedPin() const;
	bool HasHoveredPinLink() const;
	UEdGraphPin* GetHoveredOrSelectedPin(TSharedPtr<FBAGraphHandler> GraphHandler);
};

class XTOOLS_BLUEPRINTASSIST_API FBAPinActions final : public FBAPinActionsBase
{
public:
	virtual void Init() override;

	// Graph commands
	TSharedPtr<FUICommandList> PinCommands;
	TSharedPtr<FUICommandList> PinEditCommands;

	// has selected pin
	void SwapPinConnection(bool bUp);
	void LinkToHoveredPin();
	void OpenPinLinkMenu();
	void DuplicateNodeForEachLink();
	void OnEditSelectedPinValue();

	// has hovered or selected pin
	void DisconnectPinOrWire();
	void SplitPin();
	void RecombinePin();
	void StraightenHoveredPin();
};
