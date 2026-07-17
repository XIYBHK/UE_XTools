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
	Object = InObject;
	Property = InProperty;

	if (!Object || !Property)
	{
		return;
	}

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

FBAScopedPropertySetter::FBAScopedPropertySetter(
	UObject* Object,
	FName PropertyName,
	FText InTransaction,
	EPropertyChangeType::Type ChangeType,
	bool bSaveConfig)
		: Object(nullptr)
		, Property(nullptr)
		, ChangeType(EPropertyChangeType::ValueSet)
{
	if (FProperty* Prop = Object->GetClass()->FindPropertyByName(PropertyName))
	{
		FBAScopedPropertySetter(Object, Prop, InTransaction, ChangeType, bSaveConfig);
	}
}

FBAScopedPropertySetter::~FBAScopedPropertySetter()
{
	if (!Object || !Property)
	{
		return;
	}

	FPropertyChangedEvent Event(Property, ChangeType);
	Object->PostEditChangeProperty(Event);

	if (bSaveConfig)
	{
		Object->SaveConfig();
	}
}
