#include "BlueprintAssistActions/BlueprintAssistActionsBase.h"

#include "BlueprintAssistGraphHandler.h"
#include "BlueprintAssistSettings_Advanced.h"
#include "BlueprintAssistTabHandler.h"
#include "Framework/Application/SlateApplication.h"

TSharedPtr<FBAGraphHandler> FBAActionsBase::GetGraphHandler()
{
	return FBATabHandler::Get().GetActiveGraphHandler();
}

bool FBAActionsBase::CanProduceActionForCommand(const TSharedRef<const FUICommandInfo>& Command)
{
	return !GetDefault<UBASettings_Advanced>()->DisabledCommands.Contains(Command->GetCommandName());
}

bool FBAActionsBase::CanExecuteActions() const
{
	return FSlateApplication::Get().IsInitialized() && !FBAUtils::IsGamePlayingAndHasFocus();
}
