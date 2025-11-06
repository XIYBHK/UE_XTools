// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "AssetRegistry/AssetData.h"
#include "BlueprintAssistActions/BlueprintAssistBlueprintActions.h"
#include "BlueprintAssistActions/BlueprintAssistGlobalActions.h"
#include "BlueprintAssistActions/BlueprintAssistGraphActions.h"
#include "BlueprintAssistActions/BlueprintAssistNodeActions.h"
#include "BlueprintAssistActions/BlueprintAssistPinActions.h"
#include "BlueprintAssistActions/BlueprintAssistTabActions.h"
#include "BlueprintAssistActions/BlueprintAssistToolkitActions.h"
#include "BlueprintAssistMisc/BlueprintAssistInputProcessorState.h"
#include "Framework/Application/IInputProcessor.h"

class UEdGraphNode;
class SGraphPanel;

class BLUEPRINTASSIST_API FBAInputProcessor final
	: public TSharedFromThis<FBAInputProcessor>
	, public IInputProcessor
{
public:
	virtual ~FBAInputProcessor() override;

	static void Create();

	static FBAInputProcessor& Get();

	void Cleanup();

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	bool OnMouseDrag(FSlateApplication& SlateApp, const FVector2D& MousePos, const FVector2D& Delta);

	bool OnKeyOrMouseDown(FSlateApplication& SlateApp, const FKey& Key);
	bool OnKeyOrMouseUp(FSlateApplication& SlateApp, const FKey& Key);
	//~ End IInputProcessor Interface

	void HandleSlateInputEvent(const FSlateDebuggingInputEventArgs& EventArgs);

	bool BeginGroupMovement(const FKey& Key);

	FVector2D LastMousePos;

	/* Anchor node for usage in group movement */ 
	TWeakObjectPtr<UEdGraphNode> AnchorNode;
	FVector2D LastAnchorPos;

	bool bIsDisabled = false;

	bool CanExecuteCommand(TSharedRef<const FUICommandInfo> Command) const;
	bool TryExecuteCommand(TSharedRef<const FUICommandInfo> Command);
	const TArray<TSharedPtr<FUICommandList>>& GetCommandLists() { return CommandLists; }

	bool IsDisabled() const;

	void UpdateGroupMovement();
	void GroupMoveSelectedNodes(const FVector2D& Delta);
	void GroupMoveNodes(const FVector2D& Delta, TSet<UEdGraphNode*>& Nodes);

	FBANodeMovementTransaction DragNodeTransaction;

	bool IsInputChordDown(const FInputChord& Chord);
	bool IsAnyInputChordDown(const TArray<FInputChord>& Chords);
	bool IsInputChordDown(const FInputChord& Chord, const FKey Key);
	bool IsAnyInputChordDown(const TArray<FInputChord>& Chords, const FKey Key);
	bool IsKeyDown(const FKey Key);
	double GetKeyDownDuration(const FKey Key);

private:
	FBAGlobalActions GlobalActions;
	FBATabActions TabActions;
	FBAToolkitActions ToolkitActions;
	FBAGraphActions GraphActions;
	FBANodeActions NodeActions;
	FBAPinActions PinActions;
	FBABlueprintActions BlueprintActions;
	TArray<TSharedPtr<FUICommandList>> CommandLists;

	TArray<FAssetData> CutAssets;

	TSet<FKey> KeysDown;
	TMap<FKey, double> KeysDownStartTime;

	FBAInputProcessorState ProcessorState;

	FBAInputProcessor();

#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
	virtual const TCHAR* GetDebugName() const override { return TEXT("BlueprintAssistInputProcessor"); }
#endif

	bool ProcessFolderBookmarkInput();

	bool ProcessContentBrowserInput();

	void OnWindowFocusChanged(bool bIsFocused);

	bool ProcessCommandBindings(TSharedPtr<FUICommandList> CommandList, const FKeyEvent& KeyEvent);
};
