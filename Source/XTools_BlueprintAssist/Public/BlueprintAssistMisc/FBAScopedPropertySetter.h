// Copyright fpwong. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintAssistTypes.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

struct FBAScopedPropertySetter
{
	FBAScopedPropertySetter(
		UObject* Object,
		BA_PROPERTY* Property,
		FText InTransaction = FText::GetEmpty(),
		EPropertyChangeType::Type ChangeType = EPropertyChangeType::ValueSet,
		bool bSaveConfig = true);

	~FBAScopedPropertySetter();

	bool bSaveConfig = false;
	UObject* Object;
	BA_PROPERTY* Property;
	TSharedPtr<FScopedTransaction> Transaction;
	EPropertyChangeType::Type ChangeType;
};
