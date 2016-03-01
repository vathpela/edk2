/** @file

  Copyright (c) 2004  - 2014, Intel Corporation. All rights reserved.<BR>
                                                                                   
  This program and the accompanying materials are licensed and made available under
  the terms and conditions of the BSD License that accompanies this distribution.  
  The full text of the license may be found at                                     
  http://opensource.org/licenses/bsd-license.php.                                  
                                                                                   
  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,            
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.    
                                                                                   

**/
#include <PiDxe.h>
#include <Uefi.h>
#include <Guid/Capsule.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/ShellLib.h>
#include <FileHandleLib.h>
#include <Library/CapsuleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/Esrt.h>
#include <Protocol/PchPlatformPolicy.h>


extern EFI_GUID gEfiDFUVerGuid;
extern EFI_GUID gEfiPartTypeSystemPartGuid;
extern EFI_GUID gEfiFirmwareClassGuid;
extern EFI_GUID gEfiGlobalVariableGuid;
extern EFI_GUID gEfiFwDisplayCapsuleGuid;
extern EFI_GUID gEfiDFUResultGuid;

EFI_GUID KSystemFirmwareGuid = { 0x3bbdb30d, 0xaa9b, 0x4915, { 0x95, 0x03, 0xe4, 0xb8, 0x2f, 0xb6, 0xb6, 0xe9 }};
EFI_GUID KBiosCapsuleFromAfuGuid = { 0xCD193840, 0x2881, 0x9567, { 0x39, 0x28, 0x38, 0xc5, 0x97, 0x53, 0x49, 0x77 }};



#pragma pack(1)

typedef struct {
  UINT8    Version;
  UINT8    CheckSum;
  UINT8    ImageType;
  UINT8    Reserved;
  UINT32   VideoMode;
  UINT32   ImageOffsetX;
  UINT32   ImageOffsetY;
 } DISPLAY_CAPSULE_PAYLOAD;

typedef struct {
  UINT8   Blue;
  UINT8   Green;
  UINT8   Red;
  UINT8   Reserved;
} BMP_COLOR_MAP;

typedef struct {
  CHAR8         CharB;
  CHAR8         CharM;
  UINT32        Size;
  UINT16        Reserved[2];
  UINT32        ImageOffset;
  UINT32        HeaderSize;
  UINT32        PixelWidth;
  UINT32        PixelHeight;
  UINT16        Planes;          ///< Must be 1
  UINT16        BitPerPixel;     ///< 1, 4, 8, or 24
  UINT32        CompressionType;
  UINT32        ImageSize;       ///< Compressed image size in bytes
  UINT32        XPixelsPerMeter;
  UINT32        YPixelsPerMeter;
  UINT32        NumberOfColors;
  UINT32        ImportantColors;
} BMP_IMAGE_HEADER;

typedef struct {
 EFI_GUID         FwClass;
 UINT32           FwType;
 UINT32           FwVersion;
 UINT32           FwLstCompatVersion;
 UINT32           CapsuleFlags;
 UINT32           LastAttemptVersion;
 UINT32           LastAttempStatus;
} FW_RES_VAR_ENTRY;


typedef struct {
  UINT32           NumEntries;
  FW_RES_VAR_ENTRY     FwEntries[256];
} FW_RES_VAR_ENTRY_LIST;

#if 1
#define PCI_DEVICE_PATH_NODE(Func, Dev) \
  { \
    { \
      HARDWARE_DEVICE_PATH, \
      HW_PCI_DP, \
      { \
        (UINT8) (sizeof (PCI_DEVICE_PATH)), \
        (UINT8) ((sizeof (PCI_DEVICE_PATH)) >> 8) \
      } \
    }, \
    (Func), \
    (Dev) \
  }

#define PNPID_DEVICE_PATH_NODE(PnpId) \
  { \
    { \
      ACPI_DEVICE_PATH, \
      ACPI_DP, \
      { \
        (UINT8) (sizeof (ACPI_HID_DEVICE_PATH)), \
        (UINT8) ((sizeof (ACPI_HID_DEVICE_PATH)) >> 8) \
      }, \
    }, \
    EISA_PNP_ID((PnpId)), \
    0 \
  }

#define gPciRootBridge \
  PNPID_DEVICE_PATH_NODE(0x0A03)

#define gEndEntire \
  { \
    END_DEVICE_PATH_TYPE, \
    END_ENTIRE_DEVICE_PATH_SUBTYPE, \
    { \
      END_DEVICE_PATH_LENGTH, \
      0 \
    } \
  }


typedef struct {
  ACPI_HID_DEVICE_PATH      PciRootBridge;
  PCI_DEVICE_PATH           PciDevice;
  EFI_DEVICE_PATH_PROTOCOL  End;
} PLATFORM_PCI_DEVICE_PATH;

//
//For VLV2, eMMC device at BDF(0x0, 0x10, 0x0)
//
PLATFORM_PCI_DEVICE_PATH gEmmcDevPath0 = {
  gPciRootBridge,
  PCI_DEVICE_PATH_NODE (0x00, 0x10),
  gEndEntire
};

PLATFORM_PCI_DEVICE_PATH gEmmcDevPath1 = {
  gPciRootBridge,
  PCI_DEVICE_PATH_NODE (0x00, 0x17),
  gEndEntire
};

#endif

#pragma pack()

typedef struct {
  CHAR16    *FileName;
  CHAR16    *String;
} FILE_NAME_TO_STRING;


FILE_NAME_TO_STRING gFlashFileNames[] = {
  { L"\\IFWI.bin"},
};
FILE_NAME_TO_STRING gBitMapFileNames[] = {
  { L"\\INTEL.bmp"},
};
FILE_NAME_TO_STRING gCrashDumpFileNames[] = {
    { L"\\BSOD.bin"},
};
FILE_NAME_TO_STRING gBiosFvFileNames[] = {
    { L"\\BIOSUpdate.FV"},
};


#if 1
//
//Helper API to store Crash dump onto EMMC
//
EFI_STATUS
StoreCrashDumpToEmmc(
  UINT32 *FileBuffer,
  UINT32 FileSize
)

{
  EFI_STATUS                   Status;
  EFI_HANDLE                   *HandleArray;
  UINTN                                 HandleArrayCount;
  UINTN                                 Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL       *Fs;
  EFI_FILE                              *Root;
  EFI_FILE                              *FileHandle = NULL;
  EFI_DEVICE_PATH_PROTOCOL              *DevicePath;
  UINTN                          FSize = FileSize;

  //FileHandle = NULL;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPartTypeSystemPartGuid, NULL, &HandleArrayCount, &HandleArray);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // For each system partition...
  //
  for (Index = 0; Index < HandleArrayCount; Index++) {


    Status = gBS->HandleProtocol (HandleArray[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);

    if (EFI_ERROR (Status)) {
      continue;
    }

   if (CompareMem(DevicePath, &gEmmcDevPath0, sizeof(gEmmcDevPath0) - 4) != 0){
     continue;
   }
   //
    // Get the SFS protocol from the handle
    //
    Status = gBS->HandleProtocol (HandleArray[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }
    //
    // Open the root directory, get EFI_FILE_PROTOCOL
    //
    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) {
      continue;
    }


    Status = Root->Open (Root, &FileHandle, gCrashDumpFileNames[0].FileName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);


    if (EFI_ERROR (Status)) {
      continue;
    }

   if(FileHandle == NULL)
        return EFI_UNSUPPORTED;

    Status =  FileHandleWrite(
         FileHandle,
         (UINTN*)&FSize,
         FileBuffer
         );
    if(EFI_ERROR (Status)){
      return Status;
    }

    Status =  FileHandleClose (
        FileHandle
    );

   if(EFI_ERROR (Status)){
      return Status;
    }

    return Status;

  }// end of for

  return Status;
}

//Helper function to load crash dump from EMMC
EFI_STATUS
LoadCrashDumpFromEmmc(
  UINT32 *FileBuffer,
  UINT32 *FileSize
)

{
  EFI_STATUS                   Status;
  EFI_HANDLE                   *HandleArray;
  UINTN                                 HandleArrayCount;
  UINTN                                 Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL       *Fs;
  EFI_FILE                              *Root;
  EFI_FILE                              *FileHandle = NULL;
  EFI_DEVICE_PATH_PROTOCOL              *DevicePath;
  DXE_PCH_PLATFORM_POLICY_PROTOCOL *DxePlatformPchPolicy;
  BOOLEAN                          IsEmmc45 = FALSE;



  Status  = gBS->LocateProtocol (&gDxePchPlatformPolicyProtocolGuid, NULL, &DxePlatformPchPolicy);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Locate the gDxePchPlatformPolicyProtocolGuid Failed\n"));
  }else{
    if (DxePlatformPchPolicy->SccConfig->eMMCEnabled) {
      DEBUG ((EFI_D_ERROR, "A0/A1 SCC eMMC 4.41 table\n"));
      IsEmmc45 = FALSE;
    } else if (DxePlatformPchPolicy->SccConfig->eMMC45Enabled) {
      DEBUG ((EFI_D_ERROR, "B0 SCC eMMC 4.5 table\n"));
      IsEmmc45 = TRUE;
    } else{
      IsEmmc45 = FALSE;
    }
  }

  //FileHandle = NULL;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPartTypeSystemPartGuid, NULL, &HandleArrayCount, &HandleArray);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // For each system partition...
  //
  for (Index = 0; Index < HandleArrayCount; Index++) {


    Status = gBS->HandleProtocol (HandleArray[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);

    if (EFI_ERROR (Status)) {
      continue;
    }

   if (CompareMem(DevicePath, &gEmmcDevPath0, sizeof(gEmmcDevPath0) - 4) != 0){
     continue;
   }
   //
    // Get the SFS protocol from the handle
    //
    Status = gBS->HandleProtocol (HandleArray[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }
    //
    // Open the root directory, get EFI_FILE_PROTOCOL
    //
    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) {
      continue;
    }


    Status = Root->Open (Root, &FileHandle, gCrashDumpFileNames[0].FileName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, 0);


    if (EFI_ERROR (Status)) {
      continue;
    }

   if(FileHandle == NULL)
        return EFI_UNSUPPORTED;


    Status =  FileHandleRead(
         FileHandle,
         (UINTN*)&FileSize,
         FileBuffer
         );
    if(EFI_ERROR (Status)){
      return Status;
    }

    Status =  FileHandleClose (
        FileHandle
    );

   if(EFI_ERROR (Status)){
      return Status;
    }

    return Status;

  }// end of for

  return Status;
}

//
//Helper API to copy certain file to root of EMMC.
//
EFI_STATUS
FileCopyToEmmc(
  UINT32     *FileBuffer,
  UINT32     FileSize,
  IN EFI_CAPSULE_HEADER *CapsuleHeader
)

{
  EFI_STATUS                            Status;
  EFI_HANDLE                            *HandleArray;
  UINTN                                 HandleArrayCount;
  UINTN                                 Index;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL       *Fs;
  EFI_FILE                              *Root;
  EFI_FILE                              *FileHandle = NULL;
  EFI_DEVICE_PATH_PROTOCOL              *DevicePath;
  DXE_PCH_PLATFORM_POLICY_PROTOCOL *DxePlatformPchPolicy;
  BOOLEAN                          IsEmmc45 = FALSE;



  Status  = gBS->LocateProtocol (&gDxePchPlatformPolicyProtocolGuid, NULL, &DxePlatformPchPolicy);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Locate the gDxePchPlatformPolicyProtocolGuid Failed\n"));
  }else{
    if (DxePlatformPchPolicy->SccConfig->eMMCEnabled) {
      DEBUG ((EFI_D_ERROR, "A0/A1 SCC eMMC 4.41 table\n"));
      IsEmmc45 = FALSE;
    } else if (DxePlatformPchPolicy->SccConfig->eMMC45Enabled) {
      DEBUG ((EFI_D_ERROR, "B0 SCC eMMC 4.5 table\n"));
      IsEmmc45 = TRUE;
    } else{
      IsEmmc45 = FALSE;
    }
  }


  //FileHandle = NULL;

  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPartTypeSystemPartGuid, NULL, &HandleArrayCount, &HandleArray);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // For each system partition...
  //
  for (Index = 0; Index < HandleArrayCount; Index++) {


    Status = gBS->HandleProtocol (HandleArray[Index], &gEfiDevicePathProtocolGuid, (VOID **)&DevicePath);

    if (EFI_ERROR (Status)) {
      continue;
    }
   if(IsEmmc45){
     if (CompareMem(DevicePath, &gEmmcDevPath1, sizeof(gEmmcDevPath1) - 4) != 0){
       continue;
     }
   }else{
     if (CompareMem(DevicePath, &gEmmcDevPath0, sizeof(gEmmcDevPath0) - 4) != 0){
       continue;
     }

   }
   //
    // Get the SFS protocol from the handle
    //
    Status = gBS->HandleProtocol (HandleArray[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }
    //
    // Open the root directory, get EFI_FILE_PROTOCOL
    //
    Status = Fs->OpenVolume (Fs, &Root);
    if (EFI_ERROR (Status)) {
      continue;
    }

    if ((CompareGuid(&gEfiFwDisplayCapsuleGuid, &CapsuleHeader->CapsuleGuid))){
      // Creation of Bitmap file
       Status = Root->Open (Root, &FileHandle, gBitMapFileNames[0].FileName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
    }
    else if((CompareGuid(&KSystemFirmwareGuid, &CapsuleHeader->CapsuleGuid))){
       Status = Root->Open (Root, &FileHandle, gBiosFvFileNames[0].FileName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
    }
    else {
        //
        //For all other cases, don't directly store anything here.
        //
        ;
    }
    if (EFI_ERROR (Status)) {
      continue;
    }

   if(FileHandle == NULL)
        return EFI_UNSUPPORTED;

    Status =  FileHandleWrite(
         FileHandle,
         (UINTN*)&FileSize,
         FileBuffer
         );
    if(EFI_ERROR (Status)){
      return Status;
    }

    Status =  FileHandleClose (
        FileHandle
    );

   if(EFI_ERROR (Status)){
      return Status;
    }

    return Status;

  }// end of for

  return Status;
}

#endif



/**
  Convert a *.BMP graphics image to a GOP blt buffer. If a NULL Blt buffer
  is passed in a GopBlt buffer will be allocated by this routine. If a GopBlt
  buffer is passed in it will be used if it is big enough.

  @param  BmpImage      Pointer to BMP file
  @param  BmpImageSize  Number of bytes in BmpImage
  @param  GopBlt        Buffer containing GOP version of BmpImage.
  @param  GopBltSize    Size of GopBlt in bytes.
  @param  PixelHeight   Height of GopBlt/BmpImage in pixels
  @param  PixelWidth    Width of GopBlt/BmpImage in pixels

  @retval EFI_SUCCESS           GopBlt and GopBltSize are returned.
  @retval EFI_UNSUPPORTED       BmpImage is not a valid *.BMP image
  @retval EFI_BUFFER_TOO_SMALL  The passed in GopBlt buffer is not big enough.
                                GopBltSize will contain the required size.
  @retval EFI_OUT_OF_RESOURCES  No enough buffer to allocate.

**/
EFI_STATUS
CapConvertBmpToGopBlt (
  IN     VOID      *BmpImage,
  IN     UINTN     BmpImageSize,
  IN OUT VOID      **GopBlt,
  IN OUT UINTN     *GopBltSize,
     OUT UINTN     *PixelHeight,
     OUT UINTN     *PixelWidth
  )
{
  UINT8                         *Image;
  UINT8                         *ImageHeader;
  BMP_IMAGE_HEADER              *BmpHeader;
  BMP_COLOR_MAP                 *BmpColorMap;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Blt;
  UINT64                        BltBufferSize;
  UINTN                         Index;
  UINTN                         Height;
  UINTN                         Width;
  UINTN                         ImageIndex;
  BOOLEAN                       IsAllocated;

  BmpHeader = (BMP_IMAGE_HEADER *) BmpImage;

  if (BmpHeader->CharB != 'B' || BmpHeader->CharM != 'M') {
    return EFI_UNSUPPORTED;
  }

  //
  // Doesn't support compress.
  //
  if (BmpHeader->CompressionType != 0) {
    return EFI_UNSUPPORTED;
  }

  //
  // Calculate Color Map offset in the image.
  //
  Image       = BmpImage;
  BmpColorMap = (BMP_COLOR_MAP *) (Image + sizeof (BMP_IMAGE_HEADER));

  //
  // Calculate graphics image data address in the image
  //
  Image         = ((UINT8 *) BmpImage) + BmpHeader->ImageOffset;
  ImageHeader   = Image;

  //
  // Calculate the BltBuffer needed size.
  //
  BltBufferSize = MultU64x32 ((UINT64) BmpHeader->PixelWidth, BmpHeader->PixelHeight);
  //
  // Ensure the BltBufferSize * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL) doesn't overflow
  //
  if (BltBufferSize > DivU64x32 ((UINTN) ~0, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL))) {
      return EFI_UNSUPPORTED;
   }
  BltBufferSize = MultU64x32 (BltBufferSize, sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));

  IsAllocated   = FALSE;
  if (*GopBlt == NULL) {
    //
    // GopBlt is not allocated by caller.
    //
    *GopBltSize = (UINTN) BltBufferSize;
    *GopBlt     = AllocatePool (*GopBltSize);
    IsAllocated = TRUE;
    if (*GopBlt == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
  } else {
    //
    // GopBlt has been allocated by caller.
    //
    if (*GopBltSize < (UINTN) BltBufferSize) {
      *GopBltSize = (UINTN) BltBufferSize;
      return EFI_BUFFER_TOO_SMALL;
    }
  }

  *PixelWidth   = BmpHeader->PixelWidth;
  *PixelHeight  = BmpHeader->PixelHeight;

  //
  // Convert image from BMP to Blt buffer format
  //
  BltBuffer = *GopBlt;
  for (Height = 0; Height < BmpHeader->PixelHeight; Height++) {
    Blt = &BltBuffer[(BmpHeader->PixelHeight - Height - 1) * BmpHeader->PixelWidth];
    for (Width = 0; Width < BmpHeader->PixelWidth; Width++, Image++, Blt++) {
      switch (BmpHeader->BitPerPixel) {
      case 1:
        //
        // Convert 1-bit (2 colors) BMP to 24-bit color
        //
        for (Index = 0; Index < 8 && Width < BmpHeader->PixelWidth; Index++) {
          Blt->Red    = BmpColorMap[((*Image) >> (7 - Index)) & 0x1].Red;
          Blt->Green  = BmpColorMap[((*Image) >> (7 - Index)) & 0x1].Green;
          Blt->Blue   = BmpColorMap[((*Image) >> (7 - Index)) & 0x1].Blue;
          Blt++;
          Width++;
        }

        Blt--;
        Width--;
        break;

      case 4:
        //
        // Convert 4-bit (16 colors) BMP Palette to 24-bit color
        //
        Index       = (*Image) >> 4;
        Blt->Red    = BmpColorMap[Index].Red;
        Blt->Green  = BmpColorMap[Index].Green;
        Blt->Blue   = BmpColorMap[Index].Blue;
        if (Width < (BmpHeader->PixelWidth - 1)) {
          Blt++;
          Width++;
          Index       = (*Image) & 0x0f;
          Blt->Red    = BmpColorMap[Index].Red;
          Blt->Green  = BmpColorMap[Index].Green;
          Blt->Blue   = BmpColorMap[Index].Blue;
        }
        break;

      case 8:
        //
        // Convert 8-bit (256 colors) BMP Palette to 24-bit color
        //
        Blt->Red    = BmpColorMap[*Image].Red;
        Blt->Green  = BmpColorMap[*Image].Green;
        Blt->Blue   = BmpColorMap[*Image].Blue;
        break;

      case 24:
        //
        // It is 24-bit BMP.
        //
        Blt->Blue   = *Image++;
        Blt->Green  = *Image++;
        Blt->Red    = *Image;
        break;

      case 32:
        //
        // It is 24-bit BMP.
        //
        Blt->Blue   = *Image++;
        Blt->Green  = *Image++;
        Blt->Red    = *Image++;
        break;


      default:
        //
        // Other bit format BMP is not supported.
        //
        if (IsAllocated) {
          FreePool (*GopBlt);
          *GopBlt = NULL;
        }
        return EFI_UNSUPPORTED;
        break;
      };

    }

    ImageIndex = (UINTN) (Image - ImageHeader);
    if ((ImageIndex % 4) != 0) {
      //
      // Bmp Image starts each row on a 32-bit boundary!
      //
      Image = Image + (4 - (ImageIndex % 4));
    }
  }

  return EFI_SUCCESS;
}




typedef struct {
  EFI_GUID DFU_DEVICE_GUID;
  UINT32 UpdateStatus;
  UINT32 Failure;
} DFU_UPDATE_STATUS;
EFI_STATUS GetFirmwareVersion(
  IN EFI_FIRMWARE_VOLUME_HEADER * fvh,
  IN EFI_GUID * DFUVerGuid,
  OUT UINT32  * CurrVer,
  OUT UINT32  * MinAllowedVer)
{
  EFI_FFS_FILE_HEADER * pFfs = NULL;
  UINT32 FfsSize = 0;
  UINT32 * ver = NULL;
  pFfs = (EFI_FFS_FILE_HEADER *)((unsigned char *)fvh + fvh->HeaderLength);
  do {
    FfsSize = pFfs->Size[0] + (pFfs->Size[1]<<8) + (pFfs->Size[2]<<16);
    if(CompareGuid(DFUVerGuid,&(pFfs->Name))) {
      ver = (UINT32 *)((unsigned char *)pFfs + sizeof(EFI_FFS_FILE_HEADER));
      *CurrVer = *ver;
      *MinAllowedVer = *(ver + 1);
      return EFI_SUCCESS;
    }
    pFfs = (EFI_FFS_FILE_HEADER *)( (unsigned char *)pFfs + FfsSize);
  }while((unsigned char *)pFfs < ((fvh->FvLength) + (unsigned char *)fvh));
  return EFI_VOLUME_CORRUPTED;
}

/**
  Those capsule whose GUID has entry in ESRT table.

  @param CapsuleHeader   Points to a capsule header.

  @retval EFI_SUCCESS   Input capsule has ESRT entry
  @retval EFI_UNSUPPORTED  Input capsule does not have ESRT entry
**/
EFI_STATUS
EFIAPI
ESRTSupportedCapsuleImage(
  IN EFI_CAPSULE_HEADER *CapsuleHeader
  ){
   EFI_STATUS Status;
   ESRT_OPERATION_PROTOCOL   *EsrtOp;
   FW_RES_ENTRY                FwResourceEntry;

   Status = gBS->LocateProtocol (&gEfiEsrtOperationProtocolGuid, NULL, (VOID**)&EsrtOp);
   if(EFI_ERROR(Status))
   {
      return Status;
   }


   Status = EsrtOp->EsrtGetFwEntryByGuid(CapsuleHeader->CapsuleGuid, &FwResourceEntry);
   return Status;

}



/**
  Those capsules supported by the firmwares.

  @param  CapsuleHeader    Points to a capsule header.

  @retval EFI_SUCESS       Input capsule is supported by firmware.
  @retval EFI_UNSUPPORTED  Input capsule is not supported by the firmware.
**/
EFI_STATUS
EFIAPI
SupportCapsuleImage (
  IN EFI_CAPSULE_HEADER *CapsuleHeader
  ){
    if ((CompareGuid (&gEfiCapsuleGuid, &CapsuleHeader->CapsuleGuid)) ||
      (!ESRTSupportedCapsuleImage(CapsuleHeader)) ||
      (CompareGuid (&gEfiFwDisplayCapsuleGuid, &CapsuleHeader->CapsuleGuid))){
      return EFI_SUCCESS;
    }

    return EFI_UNSUPPORTED;
}



EFI_STATUS
EFIAPI
GetUpdateStatus(
  DFU_UPDATE_STATUS ** UpdateStatus
){
  EFI_STATUS Status;
  Status = gBS->LocateProtocol(
    &gEfiDFUResultGuid,
    NULL,
    (void **)UpdateStatus
    );
  if(EFI_ERROR(Status)) {
    return EFI_NOT_FOUND;
  } else {
    return EFI_SUCCESS;
  }
}


/**
  The firmware implements to process the capsule image.

  @param  CapsuleHeader         Points to a capsule header.

  @retval EFI_SUCESS            Process Capsule Image successfully.
  @retval EFI_UNSUPPORTED       Capsule image is not supported by the firmware.
  @retval EFI_VOLUME_CORRUPTED  FV volume in the capsule is corrupted.
  @retval EFI_OUT_OF_RESOURCES  Not enough memory.
**/
EFI_STATUS
EFIAPI
ProcessCapsuleImage (
  IN EFI_CAPSULE_HEADER *CapsuleHeader
  )
{
  UINT32                       Length;
  EFI_FIRMWARE_VOLUME_HEADER   *FvImage;
  EFI_FIRMWARE_VOLUME_HEADER   *ProcessedFvImage;
  EFI_STATUS                   Status;
  EFI_HANDLE                   FvProtocolHandle;
  UINT32                       FvAlignment;

  ESRT_OPERATION_PROTOCOL   *EsrtOp;
  FW_RES_ENTRY                FwResourceEntry;

  DFU_UPDATE_STATUS            *UpdateStatus;
  UINT32                    FwUpdateRequested = 0;
  UINT32                    NewVer = 0;
  UINT32                    NewMinAllowVer = 0;

  UINT32                    FileSize = 0;
  //
  //These variables are defined for Display Capsule usage.
  //
  UINT32                         *DisplayCapsuleBuffer; //Display capsule buffer excluding capsule capsule header
  VOID                           *ImageBuffer;      //Buffer of the image itself
  UINT32                         ImageSize = 0;
  INTN                           DstX = 0, DstY = 0;
  UINTN                          BltSize = 0, Height = 0, Width = 0;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Blt = NULL;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  DISPLAY_CAPSULE_PAYLOAD        *DisplayCapsuleAttr = NULL;
  UINT8                         bPassFromAFU;
  UINTN                         Size;


  Size = sizeof(bPassFromAFU);

  FvImage = NULL;
  ProcessedFvImage = NULL;
  Status  = EFI_SUCCESS;

  if (SupportCapsuleImage (CapsuleHeader) != EFI_SUCCESS) {
    return EFI_UNSUPPORTED;
  }



  if ((CompareGuid(&gEfiFwDisplayCapsuleGuid, &CapsuleHeader->CapsuleGuid))){
    //
    //Skip the capsule header, move to payload header
    //
   FileSize = (UINT32)(CapsuleHeader->CapsuleImageSize - CapsuleHeader->HeaderSize);
   DisplayCapsuleBuffer = (UINT32*)((UINTN)CapsuleHeader + CapsuleHeader->HeaderSize);
   DisplayCapsuleAttr  = (DISPLAY_CAPSULE_PAYLOAD *)DisplayCapsuleBuffer;
   ImageBuffer = (VOID*)((UINTN)DisplayCapsuleAttr + sizeof(DISPLAY_CAPSULE_PAYLOAD));
   ImageSize = CapsuleHeader->CapsuleImageSize - CapsuleHeader->HeaderSize - sizeof(DISPLAY_CAPSULE_PAYLOAD);

   //
   //Store the capsule into EMMC. Failure to store the diaplay capsule will not be handled.
   //
   Status = FileCopyToEmmc(DisplayCapsuleBuffer, FileSize, CapsuleHeader);
   if(EFI_ERROR(Status)){
       DEBUG((EFI_D_ERROR, "Fail to store display capsule onto EMMC.\n"));
   }

   //
   //Proceed to show the display capsule
   //
   Status = gBS->HandleProtocol(gST->ConsoleOutHandle, &gEfiGraphicsOutputProtocolGuid, (VOID**)&GraphicsOutput);
   if(EFI_ERROR(Status)){
       return EFI_UNSUPPORTED;
   }
   if(GraphicsOutput != NULL){
       GraphicsOutput->Mode->Mode = DisplayCapsuleAttr->VideoMode;
    DstX = DisplayCapsuleAttr->ImageOffsetX;
    DstY = DisplayCapsuleAttr->ImageOffsetY;
   }

   Status = CapConvertBmpToGopBlt(
                ImageBuffer,
                ImageSize,
                (VOID **) &Blt,
                &BltSize,
                &Height,
                &Width);
   if(EFI_ERROR(Status)){
         return Status;
   }

   if(GraphicsOutput != NULL){
         Status = GraphicsOutput->Blt(
          GraphicsOutput,
          Blt,
          EfiBltBufferToVideo,
          0,
          0,
          DstX,
          DstY,
          Width,
          Height,
          0);
      return Status;
       }else{
         Status = EFI_UNSUPPORTED;
       }
    if(Blt != NULL){
        FreePool(Blt);
    }

    return EFI_SUCCESS;

 }

    if ((CompareGuid(&KSystemFirmwareGuid, &CapsuleHeader->CapsuleGuid))){
    //
    //If this is the system firmware and not passed from AFU, store it and postpone processing to next boot.
    //
     Status = gRT->GetVariable(L"CapsuleFromAFU",
                  &KBiosCapsuleFromAfuGuid,
                  NULL,
                  &Size,
                  (VOID*)&bPassFromAFU);

     if(EFI_ERROR(Status)){//Failed to find it
         bPassFromAFU = 0;
     }

     if(!bPassFromAFU){
    }else{
      Status = gRT->SetVariable(L"CapsuleFromAFU",
                  &KBiosCapsuleFromAfuGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  0,
                  NULL);
    }

  }

    Status = gBS->LocateProtocol (&gEfiEsrtOperationProtocolGuid, NULL, (VOID**)&EsrtOp);
    if(EFI_ERROR(Status)){
        DEBUG((EFI_D_ERROR, "Cannot locate Esrt Operation Protocol.\n"));
      return Status;
    }


    //
    //First we get the FW entry. In case it does not exist, does not allow operating on ESRT table.
    //
    Status = EsrtOp->EsrtGetFwEntryByGuid(CapsuleHeader->CapsuleGuid, &FwResourceEntry);
    if(EFI_ERROR(Status)){
        DEBUG((EFI_D_ERROR,"Cannot get ESRT entry for this capsule.\n"));
    }else{
        FwUpdateRequested = 1;     //the CapsuleGuid matches one entry in ESRT table. Proceed to check the version information.
    }

  //
  // Skip the capsule header, move to the Firware Volume
  //
  FvImage = (EFI_FIRMWARE_VOLUME_HEADER *) ((UINT8 *) CapsuleHeader + CapsuleHeader->HeaderSize);
  if(FwUpdateRequested ==1){
      Status = GetFirmwareVersion(FvImage, &gEfiDFUVerGuid, &NewVer, &NewMinAllowVer);
      if(EFI_ERROR(Status)){
          DEBUG((EFI_D_ERROR, "Failed to get version information from this FW update capsule. \n"));
          return EFI_UNSUPPORTED;
      }

	  if(FwResourceEntry.FwLstCompatVersion > NewVer){
          DEBUG((EFI_D_ERROR, "Least compatible version larger than new firmware version, Abort.\r\n"));
          return EFI_ABORTED;
	  }
      //
      //Update the ESRT table first before dispatching into the capsule, to mark a failed update orginally.
      //
      FwResourceEntry.LastAttemptStatus = 1;  //By default we will mark the udpate result as "failed".
      FwResourceEntry.LastAttemptVersion = NewVer;

      Status = EsrtOp->EsrtUpdateTableEntryByGuid(CapsuleHeader->CapsuleGuid, &FwResourceEntry);
      if(EFI_ERROR(Status)){
        DEBUG((EFI_D_ERROR,"Cannot update ESRT entry.\n"));
        return Status;
      }


  }

  Length  = CapsuleHeader->CapsuleImageSize - CapsuleHeader->HeaderSize;

  while (Length != 0) {
    //
    // Point to the next firmware volume header, and then
    // call the DXE service to process it.
    //
    if (FvImage->FvLength > (UINTN) Length) {
      //
      // Notes: need to stuff this status somewhere so that the
      // error can be detected at OS runtime
      //
      Status = EFI_VOLUME_CORRUPTED;
      break;
    }

    FvAlignment = 1 << ((FvImage->Attributes & EFI_FVB2_ALIGNMENT) >> 16);
    //
    // FvAlignment must be more than 8 bytes required by FvHeader structure.
    //
    if (FvAlignment < 8) {
      FvAlignment = 8;
    }
    //
    // Check FvImage Align is required.
    //
    if (((UINTN) FvImage % FvAlignment) == 0) {
      ProcessedFvImage = FvImage;
    } else {
      //
      // Allocate new aligned buffer to store FvImage.
      //
      ProcessedFvImage = (EFI_FIRMWARE_VOLUME_HEADER *) AllocateAlignedPages ((UINTN) EFI_SIZE_TO_PAGES ((UINTN) FvImage->FvLength), (UINTN) FvAlignment);
      if (ProcessedFvImage == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }
      CopyMem (ProcessedFvImage, FvImage, (UINTN) FvImage->FvLength);
    }

    Status = gDS->ProcessFirmwareVolume (
                  (VOID *) ProcessedFvImage,
                  (UINTN) ProcessedFvImage->FvLength,
                  &FvProtocolHandle
                  );
    if (EFI_ERROR (Status)) {
      break;
    }
    //
    // Call the dispatcher to dispatch any drivers from the produced firmware volume
    //
    gDS->Dispatch ();
    //
    // On to the next FV in the capsule
    //
    Length -= (UINT32) FvImage->FvLength;
    FvImage = (EFI_FIRMWARE_VOLUME_HEADER *) ((UINT8 *) FvImage + FvImage->FvLength);


   }

   if(FwUpdateRequested){
    //
    //Get Fw update status from Update DXE driver we dispatched. The DXE driver will install the protocol for this.
    //
    Status = GetUpdateStatus(&UpdateStatus);
    if(EFI_ERROR(Status)){
        //
        //DXE driver does not report its update status to BIOS. Leave the ESRT table with failed status.
        //
        DEBUG((EFI_D_ERROR, "Capsule does not report update status.\n"));
    }else{

      if(UpdateStatus->UpdateStatus == 0){

        DEBUG((EFI_D_INFO,  "Report update status as succeed.\n"));
      }else{
        DEBUG((EFI_D_ERROR,  "Report update status as fail:%d\n", UpdateStatus->UpdateStatus));
      }

      FwResourceEntry.LastAttemptStatus = UpdateStatus->UpdateStatus;  //By default we will mark the udpate result as "failed".
      if(FwResourceEntry.LastAttemptStatus ==0){
        FwResourceEntry.FwVersion = NewVer;                //If update status is succeed, changes current version.
        FwResourceEntry.FwLstCompatVersion = NewMinAllowVer;
      }

      Status = EsrtOp->EsrtUpdateTableEntryByGuid(CapsuleHeader->CapsuleGuid, &FwResourceEntry);
      if(EFI_ERROR(Status)){
        return Status;
      }
    }


  }

  return Status;
}


