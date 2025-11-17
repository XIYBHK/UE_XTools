// Copyright fpwong. All Rights Reserved.

#include "BlueprintAssistMisc/FBAScopedPropertySetter.h"

#include "BlueprintAssistWidgets/BAEditDetailsMenu.h"

FBAScopedPropertySetter::FBAScopedPropertySetter(
	UObject* InObject,
	FProperty* InProperty,
	FText InTransaction,
	EPropertyChangeType::Type InChangeType,
	bool bInSaveConfig)
{
	Property = InProperty;
	Object = InObject;

	if (!InTransaction.IsEmpty())
	{
		Transaction = MakeShared<FScopedTransaction>(InTransaction);
	}

	bSaveConfig = bInSaveConfig;
	ChangeType = InChangeType;

	FEditPropertyChain PropertyChain;
	PropertyChain.AddHead(Property);
	Object->PreEditChange(PropertyChain);
}

FBAScopedPropertySetter::~FBAScopedPropertySetter()
{
	FPropertyChangedEvent Event(Property, ChangeType);
	Object->PostEditChangeProperty(Event);

	if (bSaveConfig)
	{
		Object->SaveConfig();
	}
}
