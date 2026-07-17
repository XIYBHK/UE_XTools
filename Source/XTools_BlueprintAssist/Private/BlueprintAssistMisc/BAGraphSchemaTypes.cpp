// Copyright fpwong. All Rights Reserved.


#include "BlueprintAssistMisc/BAGraphSchemaTypes.h"

#include "BlueprintAssistTypes.h"
#include "BlueprintAssistUtils.h"
#include "EdGraphSchema_Niagara.h"
#include "RigVMHost.h"
#include "BlueprintAssistMisc/BAControlRigUtils.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "RigVMModel/RigVMController.h"
#include "Sound/SoundCue.h"
#include "SoundCueGraph/SoundCueGraph.h"

FGuid UBAGraphSchema_Material::GetNodeGuid(UEdGraphNode* Node) const
{
	// see InitExpressionNewNode
	if (UMaterialGraphNode* MaterialNode = Cast<UMaterialGraphNode>(Node))
	{
		return MaterialNode->MaterialExpression->GetMaterialExpressionId();
	}

	return Super::GetNodeGuid(Node);
}

FString UBAGraphSchema_Niagara::GetPinDefaultValue(UEdGraphPin* Pin) const
{
	if (const UEdGraphSchema_Niagara* NiagaraSchema = Cast<UEdGraphSchema_Niagara>(Pin->GetSchema()))
	{
		FNiagaraVariable Var = NiagaraSchema->PinToNiagaraVariable(Pin, true);
		FString NiagaraDefaultValue;
		if (NiagaraSchema->TryGetPinDefaultValueFromNiagaraVariable(Var, NiagaraDefaultValue))
		{
			return NiagaraDefaultValue;
		}
	}

	return FString();
}

bool UBAGraphSchema_ControlRig::CanConnectPins(UEdGraphPin* PinA, UEdGraphPin* PinB, bool bOverrideLinks, bool bAcceptConversions, bool bAcceptHiddenPins) const
{
	if (!PinA || !PinB)
	{
		return false;
	}

	ERigVMPinDirection UserLinkDirection = ERigVMPinDirection::Output;
	if (PinA->Direction == EGPD_Input)
	{
		Swap(PinA, PinB);
		UserLinkDirection = ERigVMPinDirection::Input;
	}

	URigVMPin* VMPinA = FBAControlRigUtils::GetVMPin(PinA);
	URigVMPin* VMPinB = FBAControlRigUtils::GetVMPin(PinB);
	if (!VMPinA || !VMPinB)
	{
		return false;
	}

	FString FailureMsg;
	if (!URigVMPin::CanLink(VMPinA, VMPinB, &FailureMsg, nullptr, UserLinkDirection, false, bAcceptConversions))
	{
		UE_LOG(LogBlueprintAssist, VeryVerbose, TEXT("[%hs] Unable to link ControlRigPin: %s"), __FUNCTION__, *FailureMsg);
		return false;
	}

	return true;
}

bool UBAGraphSchema_ControlRig::TryCreateConnection(FBANodePinHandle& HandleA, FBANodePinHandle& HandleB, EBABreakMethod BreakMethod, bool bConversionAllowed, bool bTryHidden) const
{
	UEdGraphPin* PinA = HandleA.GetPin();
	UEdGraphPin* PinB = HandleB.GetPin();

	if (!PinA || !PinB)
	{
		return false;
	}

	ERigVMPinDirection UserLinkDirection = ERigVMPinDirection::Output;
	if (PinA->Direction == EGPD_Input)
	{
		Swap(PinA, PinB);
		UserLinkDirection = ERigVMPinDirection::Input;
	}

	const bool bShouldBreak = BreakMethod == EBABreakMethod::Always;
	if (!CanConnectPins(PinA, PinB, bShouldBreak, bConversionAllowed, bTryHidden))
	{
		return false;
	}

	if (URigVMController* Controller = FBAControlRigUtils::GetVMController(PinA))
	{
		URigVMPin* VMPinA = FBAControlRigUtils::GetVMPin(PinA);
		URigVMPin* VMPinB = FBAControlRigUtils::GetVMPin(PinB);

		if (VMPinA && VMPinB)
		{
			FString FailureMsg;
			bool bResult = Controller->AddLink(VMPinA, VMPinB, true, UserLinkDirection, bConversionAllowed, false, &FailureMsg);
			return bResult;
		}
	}

	return false;
}

void UBAGraphSchema_ControlRig::SetNodePosition(UEdGraphNode* Node, int32 X, int32 Y, TSharedPtr<SGraphPanel> GraphPanel) const
{
	// // call set position without moving the graph node (on control rig the undo for GraphNode::MoveTo doesn't work)
	// Super::SetNodePosition(Node, X, Y, {});

	if (const URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(Node))
	{
		if (URigVMController* Controller = RigNode->GetController())
		{
			if (IsValid(Controller))
			{
				Controller->SetNodePosition(RigNode->GetModelNode(), FVector2D(X, Y), true, true);
			}
		}
	}
}

bool UBAGraphSchema_ControlRig::SetPinDefaultValue(UEdGraphPin* Pin, const FString& NewDefault, FString* OutError) const
{
#if BA_UE_VERSION_OR_LATER(5, 4)
	if (auto Controller = FBAControlRigUtils::GetVMController(Pin))
	{
		if (auto VMPin = FBAControlRigUtils::GetVMPin(Pin))
		{
			FRigVMDefaultValueTypeGuard _(Controller, ERigVMPinDefaultValueType::KeepValueType);
			return Controller->SetPinDefaultValue(VMPin, NewDefault, true, true, true);
		}
	}

	return false;
#else
	return Super::SetPinDefaultValue(Pin, NewDefault, OutError);
#endif
}

FString UBAGraphSchema_ControlRig::GetPinDefaultValue(UEdGraphPin* Pin) const
{
#if BA_UE_VERSION_OR_LATER(5, 4)
	if (auto Controller = FBAControlRigUtils::GetVMController(Pin))
	{
		if (auto VMPin = FBAControlRigUtils::GetVMPin(Pin))
		{
			if (VMPin->CanProvideDefaultValue())
			{
				return Controller->GetPinDefaultValue(VMPin->GetPinPath());
			}
		}
	}

	return FString();
#else
	return Super::GetPinDefaultValue(Pin);
#endif
}

FGuid UBAGraphSchema_ControlRig::GetNodeGuid(UEdGraphNode* Node) const
{
	if (const URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(Node))
	{
		static TMap<FName, FGuid> ParsedGuids;
		const FName ModelName = RigNode->GetModelNodeName();
		if (ParsedGuids.Contains(ModelName))
		{
			return ParsedGuids[ModelName];
		}

		const FString ModelNameStr = ModelName.ToString();

		// copied from UTexture::SetDeterministicLightingGuid
		{
			// Compute a 128-bit hash based on the name and use that as a GUID to fix this issue.
			FTCHARToUTF8 Converted(*ModelNameStr);
			FMD5 MD5Gen;
			MD5Gen.Update((const uint8*)Converted.Get(), Converted.Length());
			uint32 Digest[4];
			MD5Gen.Final((uint8*)Digest);

			// FGuid::NewGuid() creates a version 4 UUID (at least on Windows), which will have the top 4 bits of the
			// second field set to 0100. We'll set the top bit to 1 in the GUID we create, to ensure that we can never
			// have a collision with textures which use implicitly generated GUIDs.
			Digest[1] |= 0x80000000;
			FGuid Guid(Digest[0], Digest[1], Digest[2], Digest[3]);
			ParsedGuids.Add(ModelName, Guid);
			return Guid;
		}
	}

	return Super::GetNodeGuid(Node);
}

void UBAGraphSchema_SoundCue::PostGraphChanged(UEdGraph* Graph) const
{
	CastChecked<USoundCueGraph>(Graph)->GetSoundCue()->CompileSoundNodesFromGraphNodes();
}
