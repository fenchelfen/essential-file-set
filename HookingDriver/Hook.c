#include "Hook.h"
#include "Logger.h"


EFI_STATUS EFIAPI ReadBlocksRandomStaff(IN EFI_BLOCK_IO_PROTOCOL * This, IN UINT32 MediaId, IN EFI_LBA Lba, IN UINTN BufferSize, OUT VOID * Buffer);

VOID PerformHook(
	VOID
)
/*
	Hook all available instances of BlockIo
*/
{
	UINTN	NoBlkIoHandles;
	EFI_HANDLE *BlkIoHandle = NULL;
	EFI_BLOCK_IO_PROTOCOL* BlkIo;

	CHAR16 FileName[] = L"AAAMyLogFile.txt";

	if (EFI_SUCCESS == DoesFileExist(FileName)) {
		Print(L"PerformHook: HookingDriver has been loaded before, abort\r\n");
		return;
	}

	EFI_STATUS Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiBlockIoProtocolGuid,
		NULL,
		&NoBlkIoHandles,
		&BlkIoHandle
	);

	if (EFI_ERROR(Status))
		return;

	for (UINTN Index = 0; Index < NoBlkIoHandles; Index++) {
		Status = gBS->HandleProtocol(
			BlkIoHandle[Index],
			&gEfiBlockIoProtocolGuid,
			(VOID**)&BlkIo
		);

		if (EFI_ERROR(Status))
			break;

		if (BlkIo->Media->LogicalPartition)
			continue;

		// PARTITION_PRIVATE_DATA *Private = PARTITION_DEVICE_FROM_BLOCK_IO_THIS (This);
		// DEBUG((EFI_D_INFO, "Incoming signature 1 %x\r\n", BASE_CR(This, PARTITION_PRIVATE_DATA, BlockIo)->Signature));
		// DEBUG((EFI_D_INFO, "Incoming signature 2 %x\r\n", Private->Signature));

		// Was this hooked already? 
		if (ReadBlocksRandomStaff == BlkIo->ReadBlocks)
			continue;

		pHookingContext context = AllocatePool(sizeof(HookingContext));

		// Save context
		context->originalReadPtr = BlkIo->ReadBlocks;
		context->originalWritePtr = BlkIo->WriteBlocks;
		context->blkIoHandle = BlkIoHandle[Index];

		// Perform the hook
		BlkIo->ReadBlocks = ReadBlocksRandomStaff;
		// BlkIo->WriteBlocks = WriteBlocksRandomStaff;

		ht_set(gHashmap, BlkIo, context);

		Print(L"Hook: BlkIo %x: Replaced %x with %x\r\n", BlkIo, context->originalReadPtr, BlkIo->ReadBlocks); // with ReadBlocksRandomStaff
	}

	// DEBUG((EFI_D_INFO, "<HASHTABLE DUMP>\r\n"));
	// ht_dump(gHashmap);

	if (EFI_ERROR(Status))
		return;
}

VOID PerformUnhook(
	VOID
)
/*
	Unhook all available instances of BlockIo
*/
{
	UINTN	NoBlkIoHandles;
	EFI_HANDLE *BlkIoHandle = NULL;
	EFI_BLOCK_IO_PROTOCOL* BlkIo;

	EFI_STATUS Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiBlockIoProtocolGuid,
		NULL,
		&NoBlkIoHandles,
		&BlkIoHandle
	);

	if (EFI_ERROR(Status)) {
		Print(L"Unhook: Problems locating handle buffer: %r\r\n", Status);
		return;
	}

	for (UINTN Index = 0; Index < NoBlkIoHandles; Index++) {
		Status = gBS->HandleProtocol(
			BlkIoHandle[Index],
			&gEfiBlockIoProtocolGuid,
			(VOID**)&BlkIo
		);

		if (EFI_ERROR(Status))
			break;

		if (BlkIo->Media->LogicalPartition)
			continue;

		// PARTITION_PRIVATE_DATA *Private = PARTITION_DEVICE_FROM_BLOCK_IO_THIS (This);
		// DEBUG((EFI_D_INFO, "Incoming signature 1 %x\r\n", BASE_CR(This, PARTITION_PRIVATE_DATA, BlockIo)->Signature));
		// DEBUG((EFI_D_INFO, "Incoming signature 2 %x\r\n", Private->Signature));

		pHookingContext context = ht_get(gHashmap, BlkIo);

		// Perhaps PerformHook has not been called before, avoid this BlkIo 
		if (NULL == context)
			continue;

		// Replace hooked function with the original ones
		BlkIo->ReadBlocks = context->originalReadPtr;
		BlkIo->WriteBlocks = context->originalWritePtr;

		context->blkIoHandle = BlkIoHandle[Index];

		Print(L"Unhook: BlkIo %x: Replace %x with %x\r\n", BlkIo, BlkIo->ReadBlocks, context->originalReadPtr);
		// Print(L"Unhook: BlkIo %x: Replace %x with %x\r\n", BlkIo, BlkIo->WriteBlocks, context->originalWritePtr);
	}

	// DEBUG((EFI_D_INFO, "<HASHTABLE DUMP>\r\n"));
	// ht_dump(gHashmap);
}

VOID NotifyHook(IN EFI_EVENT Event, IN VOID * context)
/*
	Triggered on UEFI unload to unhook BlockIo
*/
{
	PerformUnhook();
	DumpLogToFile();
}

EFI_STATUS
EFIAPI
ReadBlocksRandomStaff(
	IN EFI_BLOCK_IO_PROTOCOL* This,
	IN UINT32                 MediaId,
	IN EFI_LBA                Lba,
	IN UINTN                  BufferSize,
	OUT VOID*                 Buffer
)
{
	DEBUG((EFI_D_INFO, "Jesus, I'm reading blocks random staff! <INSIDE>, poarg list: %x %x %x\r\n", MediaId, Lba, BufferSize));

	pHookingContext context = ht_get(gHashmap, This);
	// Print(L"context->originalReadPtr %x, ReadBlocksRandomStaff %x\r\n", context->originalReadPtr, ReadBlocksRandomStaff);
	Print(L"ReadBlocksRandomStaff: LBA %x      BufferSize      %x\r\n", Lba, BufferSize);
	// RetrieveGUID(context->blkIoHandle, This, context);

	// TODO: AppendToLog causes infinite recursion, unhook and flush to avoid
	// AppendToLog(This, MediaId, Lba, BufferSize, Buffer, TRUE);
	AppendToLog(This, MediaId, Lba, BufferSize, Buffer, TRUE, context->blkIoHandle, This, context);

	return context->originalReadPtr(This, MediaId, Lba, BufferSize, Buffer);
}


EFI_STATUS
EFIAPI
WriteBlocksRandomStaff(
	IN EFI_BLOCK_IO_PROTOCOL *This,
	IN UINT32                 MediaId,
	IN EFI_LBA                Lba,
	IN UINTN                  BufferSize,
	IN VOID                   *Buffer
)
{
	//CHAR16* MyString = L"Write override\r\n";

	//if (0 != StrCmp(Buffer, MyString)) 
	//	gST->ConOut->OutputString(gST->ConOut, MyString);

	DEBUG((EFI_D_INFO, "Jesus, I'm writing blocks random staff! <INSIDE>\r\n"));
	// AppendToLog(MediaId, Lba, BufferSize, Buffer);

	// return WriteBlocksOrigAddress(This, MediaId, Lba, BufferSize, Buffer);
	return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
RetrieveGUID(
	IN EFI_HANDLE				    BlkIoHandle,
	IN EFI_BLOCK_IO_PROTOCOL		*BlkIo,
	IN pHookingContext				context,
	OUT EFI_GUID *diskGuid
)
{
	EFI_STATUS                  Status;
	UINT32                      BlockSize;
	EFI_DEVICE_PATH_PROTOCOL    *DevPath;
	CHAR16                      *DevPathString;
	MASTER_BOOT_RECORD          *PMBR;
	EFI_PARTITION_TABLE_HEADER  *PartHdr;

	//
	// Locate Handles that support BlockIo protocol
	//

	if (BlkIo->Media->LogicalPartition) {  // skip if partition
		return 0;
	}
	DevPath = DevicePathFromHandle(BlkIoHandle);
	if (DevPath == NULL) {
		return 0;
	}

	DevPathString = ConvertDevicePathToText(DevPath, TRUE, FALSE);

	BlockSize = BlkIo->Media->BlockSize;
	PartHdr = AllocateZeroPool(BlockSize);
	PMBR = AllocateZeroPool(BlockSize);

	// read LBA0
	Status = context->originalReadPtr(
		BlkIo,
		BlkIo->Media->MediaId,
		(EFI_LBA)0,							// LBA 0, MBR/Protective MBR
		BlockSize,
		PMBR
	);
	// read LBA1
	Status = context->originalReadPtr(
		BlkIo,
		BlkIo->Media->MediaId,
		(EFI_LBA)1,							// LBA 1, GPT
		BlockSize,
		PartHdr
	);

	// check if GPT
	if (PartHdr->Header.Signature == EFI_PTAB_HEADER_ID) {

		if (PMBR->Signature == MBR_SIGNATURE) {
			DEBUG((EFI_D_INFO, "RetrieveGuid: Found protective MBR\r\n"));
		}
		DEBUG((EFI_D_INFO, "RetrieveGuid: GPT PartHdr=%x\r\n", PartHdr));
		CopyMem(diskGuid, &PartHdr->DiskGUID, sizeof(diskGuid));
	}
	else if (PMBR->Signature == MBR_SIGNATURE) {
		DEBUG((EFI_D_INFO, "RetrieveGuid: MBR PartHdr=%x\r\n", PartHdr));
	}

	// todo: Read partition data and save to log

	FreePool(PartHdr);
	FreePool(PMBR);
	return Status;
}

