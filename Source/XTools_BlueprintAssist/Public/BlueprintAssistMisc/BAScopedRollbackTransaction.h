// Copyright fpwong. All Rights Reserved.

#pragma once

#include "ScopedTransaction.h"

/* 
 * Scoped transaction with rollback function
 * TODO Say you start transaction A going then start transaction B. If you try to rollback B while both are ongoing it will hit an ensure.
 */
class XTOOLS_BLUEPRINTASSIST_API FBAScopedRollbackTransaction : public FScopedTransaction
{
public:
	FBAScopedRollbackTransaction(const FText& SessionName, const bool bShouldActuallyTransact = true)
		: FScopedTransaction(SessionName, bShouldActuallyTransact)
	{
	}

	void Rollback(const FText& OptionalFailureMsg = FText::GetEmpty());
};
