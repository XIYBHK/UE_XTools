// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "BlueprintAssistMisc/IBAInputState.h"
#include "Framework/Application/IInputProcessor.h"

struct FSlateDebuggingInputEventArgs;
class FUICommandList;
class FUICommandInfo;
class FBANodeActions;
class FBAInputProcessorState;
class FBABlueprintActions;
class FBAPinActions;
class FBAGraphActions;
class FBAToolkitActions;
class FBATabActions;
class FBAGlobalActions;
class UEdGraphNode;
class SGraphPanel;

struct FBAShakeTracking
{
	TWeakObjectPtr<UEdGraphNode> Node;
	int32 Count = 0;
	double LastTime = 0.0;
	FVector2D LastDirection = FVector2D::ZeroVector;
};

class XTOOLS_BLUEPRINTASSIST_API FBAInputProcessor final
	: public TSharedFromThis<FBAInputProcessor>
	, public IInputProcessor
	, public IBAInputState
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
	bool TryProcessShakeNodeOffWire(const FVector2D& Delta);

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

	virtual bool IsInputChordDown(const FInputChord& Chord) override;
	virtual bool IsAnyInputChordDown(const TArray<FInputChord>& Chords) override;
	virtual bool IsInputChordDown(const FInputChord& Chord, const FKey Key) override;
	virtual bool IsAnyInputChordDown(const TArray<FInputChord>& Chords, const FKey Key) override;
	virtual bool IsKeyDown(const FKey Key) override;
	virtual double GetKeyDownDuration(const FKey Key) override;

	TUniquePtr<FBAGlobalActions> GlobalActions;
	TUniquePtr<FBATabActions> TabActions;
	TUniquePtr<FBAToolkitActions> ToolkitActions;
	TUniquePtr<FBAGraphActions> GraphActions;
	TUniquePtr<FBANodeActions> NodeActions;
	TUniquePtr<FBAPinActions> PinActions;
	TUniquePtr<FBABlueprintActions> BlueprintActions;

private:
	TArray<TSharedPtr<FUICommandList>> CommandLists;

	TSet<FKey> KeysDown;
	TMap<FKey, double> KeysDownStartTime;

	TUniquePtr<FBAInputProcessorState> ProcessorState;
	FBAShakeTracking ShakeTracking;

	FBAInputProcessor();

#if ENGINE_MINOR_VERSION >= 26 || ENGINE_MAJOR_VERSION >= 5
	virtual const TCHAR* GetDebugName() const override { return TEXT("BlueprintAssistInputProcessor"); }
#endif

	bool ProcessFolderBookmarkInput();

	void OnWindowFocusChanged(bool bIsFocused);

	bool ProcessCommandBindings(TSharedPtr<FUICommandList> CommandList, const FKeyEvent& KeyEvent);
};
