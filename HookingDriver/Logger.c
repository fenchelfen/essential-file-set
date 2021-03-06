#include "Logger.h"
#include <Library/DebugLib.h>
#include <Guid/FileInfo.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <IndustryStandard/Mbr.h>


EFI_STATUS DoesFileExist(
	CHAR16 *FileName
)
/*
	Return EFI_SUCCESS if file exists
*/
{
	EFI_STATUS status = 0;
	EFI_FILE_PROTOCOL *Fs;
	EFI_FILE_PROTOCOL *File = NULL;

	status = FindWritableFs(&Fs);

	if (EFI_ERROR(status)) {
		DEBUG((EFI_D_ERROR, "DoesFileExist: Can't find writable FS\n"));
		return EFI_SUCCESS;
	}

	status = Fs->Open(Fs, &File, FileName, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(status)) {
		return status;
	}
	return EFI_SUCCESS;
}

EFI_STATUS
DumpLogToFile(
	VOID
)
{
	EFI_STATUS status = 0;
	EFI_FILE_PROTOCOL *Fs;
	EFI_FILE_PROTOCOL *File = NULL;
	CHAR16 FileName[] = L"AAAMyLogFile.txt";

	// Find a writable FS
	status = FindWritableFs(&Fs);

	if (EFI_ERROR(status)) {
		DEBUG((EFI_D_ERROR, "DumpLogToFile: Can't find writable FS\n"));
		return EFI_SUCCESS;
	}

	// Open or create an output file
	status = Fs->Open(Fs, &File, FileName, EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
	if (EFI_ERROR(status)) {
		DEBUG((EFI_D_ERROR, "DumpLogToFile: Fs->Open of %s returned %r\n", FileName, status));
		return status;
	}

	while (!list_empty(gLog)) {
		UINT64 *buffer = list_dequeue(gLog);
		status = WriteDataToFile(
			buffer,
			sizeof(UINT64) * 6,
			File
		);

		if (EFI_ERROR(status)) {
			DEBUG((EFI_D_ERROR, "DumpLogToFile: WriteDataToFile of %s returned %r\n", buffer, status));
			return status;
		}
	}

	File->Flush(File);
	File->Close(File);

	return EFI_SUCCESS;
}

// #pragma optimize( "", off )

EFI_STATUS
AppendToLog(
	IN EFI_BLOCK_IO_PROTOCOL* BlockIo,
	IN UINT32 MediaId,
	IN EFI_LBA Lba,
	IN UINTN BufferSize,
	IN VOID *Buffer,
	IN BOOLEAN isRead,

	IN EFI_HANDLE BlkIoHandle,
	IN EFI_BLOCK_IO_PROTOCOL *BlkIo,
	IN pHookingContext context
)
{
	UINT64 *message; // = { 0 }; // (UINT64) Lba, BufferSize, '\n' };

	message = AllocatePool(6 * sizeof(UINT64));

	message[0] = (UINT64)isRead | (UINT64)MediaId << 32;
	message[1] = (UINT64)Lba;
	message[2] = (UINT64)BufferSize;
	message[3] = (UINT64)0x7777777777777777;
	message[4] = (UINT64)0x7777777777777777;
	message[5] = (UINT64)0x7777777777777777;

	// message[0] = (UINT64)0x3333333333333333;
	// message[1] = (UINT64)0x4444444444444444;
	// message[2] = (UINT64)0x5555555555555555;
	// message[3] = (UINT64)0x7777777777777777;

	list_enqueue(gLog, (VOID *)message);
	// UINT64 *buffer = (UINT64*)list_dequeue(gLog);
	// DEBUG((EFI_D_INFO, "message: %s, buffer ptr %x\r\n", message, buffer));

	// todo: Retrieve GUID and store it in the log
	// todo: pass EFI_HANDLE BlockIoHandle into the function too
	EFI_GUID *diskGuid = (EFI_GUID *)AllocateZeroPool(sizeof(diskGuid));

	RetrieveGUID(BlkIoHandle, BlkIo, context, diskGuid);

	message[4] = diskGuid->Data1;

	// return (EFI_STATUS)buffer;
	return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
FindWritableFs(
	OUT EFI_FILE_PROTOCOL **WritableFs
)
{
	EFI_HANDLE *HandleBuffer = NULL;
	UINTN      HandleCount;
	UINTN      i;

	// Locate all the simple file system devices in the system
	EFI_STATUS Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleCount, &HandleBuffer);
	if (!EFI_ERROR(Status)) {
		EFI_FILE_PROTOCOL *Fs = NULL;
		// For each located volume
		for (i = 0; i < HandleCount; i++) {
			EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs = NULL;
			EFI_FILE_PROTOCOL *File = NULL;

			// Get protocol pointer for current volume
			Status = gBS->HandleProtocol(HandleBuffer[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&SimpleFs);
			if (EFI_ERROR(Status)) {
				DEBUG((EFI_D_ERROR, "FindWritableFs: gBS->HandleProtocol[%d] returned %r\n", i, Status));
				continue;
			}

			// Open the volume
			Status = SimpleFs->OpenVolume(SimpleFs, &Fs);
			if (EFI_ERROR(Status)) {
				DEBUG((EFI_D_ERROR, "FindWritableFs: SimpleFs->OpenVolume[%d] returned %r\n", i, Status));
				continue;
			}

			// Try opening a file for writing
			Status = Fs->Open(Fs, &File, L"crsdtest.fil", EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
			if (EFI_ERROR(Status)) {
				DEBUG((EFI_D_ERROR, "FindWritableFs: Fs->Open[%d] returned %r\n", i, Status));
				continue;
			}

			// Writable FS found
			Fs->Delete(File);
			*WritableFs = Fs;
			Status = EFI_SUCCESS;
			break;
		}
	}

	// Free memory
	if (HandleBuffer) {
		gBS->FreePool(HandleBuffer);
	}

	return Status;
}

EFI_STATUS
WriteDataToFile(
	IN VOID* Buffer,
	IN UINTN BufferSize,
	IN EFI_FILE_PROTOCOL* File
)
{
	UINTN infoBufferSize = 0;
	EFI_FILE_INFO* fileInfo = NULL;

	//  retrieve file info to know its size
	EFI_STATUS status = File->GetInfo(
		File,
		&gEfiFileInfoGuid,
		&infoBufferSize,
		(VOID*)fileInfo
	);

	if (EFI_BUFFER_TOO_SMALL != status)
	{
		return status;
	}

	fileInfo = AllocatePool(infoBufferSize);

	if (NULL == fileInfo)
	{
		status = EFI_OUT_OF_RESOURCES;
		return status;
	}

	//    we need to know file size
	status = File->GetInfo(
		File,
		&gEfiFileInfoGuid,
		&infoBufferSize,
		(VOID*)fileInfo
	);

	if (EFI_ERROR(status))
	{
		goto FINALLY;
	}

	// we move carriage to the end of the file
	status = File->SetPosition(
		File,
		fileInfo->FileSize
	);

	if (EFI_ERROR(status))
	{
		goto FINALLY;
	}

	// write buffer
	status = File->Write(
		File,
		&BufferSize,
		Buffer
	);

	if (EFI_ERROR(status))
	{
		goto FINALLY;
	}

	//    flush data
	status = File->Flush(File);

FINALLY:

	if (NULL != fileInfo)
	{
		FreePool(fileInfo);
	}

	return status;
}

EFI_STATUS
AppPrintBuffer(
	CHAR16  *Buffer
)
{
	UINTN   i;

	for (i = 0; i <= 0xFF; i++) {
		if ((i % 10) == 0) {
			if (i != 0);
			DEBUG((EFI_D_INFO, "\r\n"));
			DEBUG((EFI_D_INFO, "%.3d: ", i));
		}

		DEBUG((EFI_D_INFO, "%.4X ", Buffer[i]));
	}
	return EFI_SUCCESS;
}

