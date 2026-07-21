/* Copyright (C) 2024 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the Unreal Engine Marketplace
*/

#pragma once

#if PLATFORM_WINDOWS && !UE_BUILD_SHIPPING
#include "Windows/AllowWindowsPlatformTypes.h"

struct FHotPatch
{
	static constexpr SIZE_T PatchSize = 13;

	~FHotPatch()
	{
		Unhook();
	}

	template <typename FunctionType>
	bool Hook(FunctionType* From, FunctionType* To)
	{
		if (bInstalled || !From || !To)
		{
			return false;
		}

		uint8* FromAddress = reinterpret_cast<uint8*>(From);
		uint64* ToAddress = reinterpret_cast<uint64*>(To);

		uint8 Patch[] =
		{
			0x49, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x41, 0xFF, 0xE2
		};

		FMemory::Memcpy(&Patch[2], &ToAddress, sizeof(ToAddress));

		FMemory::Memcpy(OriginalBytes, FromAddress, PatchSize);

		DWORD BaseProtection = 0;
		const DWORD NewProtection = PAGE_EXECUTE_READWRITE;
		if (!VirtualProtect(FromAddress, PatchSize, NewProtection, &BaseProtection))
		{
			return false;
		}

		FMemory::Memcpy(FromAddress, Patch, PatchSize);
		PatchedAddress = FromAddress;
		bInstalled = true;

		DWORD RestoreProtection = 0;
		if (!VirtualProtect(FromAddress, PatchSize, BaseProtection, &RestoreProtection))
		{
			return false;
		}
		FlushInstructionCache(GetCurrentProcess(), FromAddress, PatchSize);

		return true;
	}

	bool Unhook()
	{
		if (!bInstalled || !PatchedAddress)
		{
			return true;
		}

		DWORD BaseProtection = 0;
		if (!VirtualProtect(PatchedAddress, PatchSize, PAGE_EXECUTE_READWRITE, &BaseProtection))
		{
			return false;
		}

		FMemory::Memcpy(PatchedAddress, OriginalBytes, PatchSize);
		DWORD RestoreProtection = 0;
		const bool bRestored = VirtualProtect(PatchedAddress, PatchSize, BaseProtection, &RestoreProtection) != 0;
		FlushInstructionCache(GetCurrentProcess(), PatchedAddress, PatchSize);
		if (bRestored)
		{
			PatchedAddress = nullptr;
			bInstalled = false;
		}

		return bRestored;
	}

private:
	uint8 OriginalBytes[PatchSize] = {};
	uint8* PatchedAddress = nullptr;
	bool bInstalled = false;
};

#include "Windows/HideWindowsPlatformTypes.h"
#endif
