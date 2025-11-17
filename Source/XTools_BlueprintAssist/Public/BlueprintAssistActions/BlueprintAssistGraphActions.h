#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTabActions.h"

class UEdGraphPin;
class FUICommandList;

class XTOOLS_BLUEPRINTASSIST_API FBAGraphActionsBase : public FBATabActionsBase
{
public:
	bool HasGraph() const;
	bool HasGraphNonReadOnly() const;
	TSharedPtr<FBAGraphHandler> GetGraphHandlerChecked();
};

class XTOOLS_BLUEPRINTASSIST_API FBAGraphActions final : public FBAGraphActionsBase
{
public:
	virtual void Init() override;

	static void OpenContextMenu(const FVector2D& MenuLocation, const FVector2D& NodeSpawnPosition);
	static void OpenContextMenuFromPin(UEdGraphPin* Pin, const FVector2D& MenuLocation, const FVector2D& NodeLocation);

	// Graph commands
	TSharedPtr<FUICommandList> GraphCommands;
	void OnFormatAllEvents() const;
	void OnOpenContextMenu();
	void CreateRerouteNode();
	void SaveAndFormat();

	// Graph read only commands
	TSharedPtr<FUICommandList> GraphReadOnlyCommands;
	void FocusGraphPanel();

protected:
	FDelegateHandle SaveAndFormatHandle;
};
