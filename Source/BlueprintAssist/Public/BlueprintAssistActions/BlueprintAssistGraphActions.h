#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistGlobals.h"
#include "BlueprintAssistTabActions.h"

class UEdGraphPin;
class FUICommandList;

class BLUEPRINTASSIST_API FBAGraphActionsBase : public FBATabActionsBase
{
public:
	bool HasGraph() const;
	bool HasGraphNonReadOnly() const;
};

class BLUEPRINTASSIST_API FBAGraphActions final : public FBAGraphActionsBase
{
public:
	virtual void Init() override;

	static void OpenContextMenu(const FBAVector2& MenuLocation, const FBAVector2& NodeSpawnPosition);
	static void OpenContextMenuFromPin(UEdGraphPin* Pin, const FBAVector2& MenuLocation, const FBAVector2& NodeLocation);

	// Graph commands
	TSharedPtr<FUICommandList> GraphCommands;
	void OnFormatAllEvents() const;
	void OnOpenContextMenu();
	void CreateRerouteNode();

	// Graph read only commands
	TSharedPtr<FUICommandList> GraphReadOnlyCommands;
	void FocusGraphPanel();
};
