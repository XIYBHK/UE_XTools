#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistNodeActions.h"

class FUICommandList;

class BLUEPRINTASSIST_API FBAPinActionsBase : public FBANodeActionsBase
{
public:
	bool HasSelectedPin() const;
	bool HasEditablePin() const;
	bool HasHoveredPin() const;
	bool HasHoveredOrSelectedPin() const;
};

class BLUEPRINTASSIST_API FBAPinActions final : public FBAPinActionsBase
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
};
