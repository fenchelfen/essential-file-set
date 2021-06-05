#pragma once

#include <UEFI.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/BlockIo.h>
#include <Partition.h>

#include "Logger.h"
#include "../hashmap.h"

extern ht_t* gHashmap;

VOID PerformHook(VOID);


typedef struct HookingContext {
	// todo: store guids here
	EFI_HANDLE blkIoHandle;
	EFI_BLOCK_READ originalReadPtr;
	EFI_BLOCK_WRITE originalWritePtr;
} HookingContext, *pHookingContext;

EFI_STATUS EFIAPI RetrieveGUID(
	IN EFI_HANDLE				    BlkIoHandle,
	IN EFI_BLOCK_IO_PROTOCOL		*BlkIo,
	IN pHookingContext				context
);

