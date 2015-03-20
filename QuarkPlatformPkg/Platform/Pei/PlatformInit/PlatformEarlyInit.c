/** @file
  This PEIM initialize platform for MRC, following action is performed,
    1. Initizluize GMCH
    2. Detect boot mode
    3. Detect video adapter to determine whether we need pre allocated memory 
    4. Calls MRC to initialize memory and install a PPI notify to do post memory initialization.
  This file contains the main entrypoint of the PEIM.
  
Copyright (c) 2013 Intel Corporation.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.
* Neither the name of Intel Corporation nor the names of its
contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**/


#include "CommonHeader.h"
#include "PlatformEarlyInit.h"
#include "PeiFvSecurity.h"

EFI_STATUS
EFIAPI
EndOfPeiSignalPpiNotifyCallback (
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  );

//
// Function prototypes to routines implemented in other source modules
// within this component.
//

EFI_STATUS
EFIAPI
PlatformErratasPostMrc (
  VOID
  );

//
// The global indicator, the FvFileLoader callback will modify it to TRUE after loading PEIM into memory
//
BOOLEAN ImageInMemory = FALSE;

BOARD_LEGACY_GPIO_CONFIG      mBoardLegacyGpioConfigTable[]  = { PLATFORM_LEGACY_GPIO_TABLE_DEFINITION };
UINTN                         mBoardLegacyGpioConfigTableLen = (sizeof(mBoardLegacyGpioConfigTable) / sizeof(BOARD_LEGACY_GPIO_CONFIG));

STATIC EFI_PEI_PPI_DESCRIPTOR mPpiBootMode[1] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiMasterBootModePpiGuid,
    NULL
  }
};  

EFI_PEI_NOTIFY_DESCRIPTOR mMemoryDiscoveredNotifyList[1] = {
  { 
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiMemoryDiscoveredPpiGuid,
    MemoryDiscoveredPpiNotifyCallback 
  }
};

EFI_PEI_NOTIFY_DESCRIPTOR mEndOfPeiSignalPpiNotifyList[1] = {
  { 
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiEndOfPeiSignalPpiGuid,
    EndOfPeiSignalPpiNotifyCallback
  }
};

EFI_PEI_STALL_PPI mStallPpi = {
  PEI_STALL_RESOLUTION,
  Stall
};

EFI_PEI_PPI_DESCRIPTOR mPpiStall[1] = {
  {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiStallPpiGuid,
    &mStallPpi
  }
};

/**
  This is the entrypoint of PEIM
  
  @param  FileHandle  Handle of the file being invoked.
  @param  PeiServices Describes the list of possible PEI Services.

  @retval EFI_SUCCESS if it completed successfully.  
**/
EFI_STATUS
EFIAPI
PeiInitPlatform (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                              Status;
  EFI_BOOT_MODE                           BootMode;
  EFI_PEI_STALL_PPI                       *StallPpi;
  EFI_PEI_PPI_DESCRIPTOR                  *StallPeiPpiDescriptor;   
  EFI_FV_FILE_INFO                        FileInfo;
  EFI_PLATFORM_INFO                       *PlatformInfo;
  EFI_HOB_GUID_TYPE                       *GuidHob;
  EFI_PLATFORM_TYPE                       PlatformType;

  GuidHob = GetFirstGuidHob (&gEfiPlatformInfoGuid);
  PlatformInfo  = GET_GUID_HOB_DATA (GuidHob);
  ASSERT (PlatformInfo != NULL);
  PlatformType = (EFI_PLATFORM_TYPE) PlatformInfo->Type;

  //
  // Initialize Firmware Volume security.
  // This must be done before any firmware volume accesses (excl. BFV)
  //
  Status = PeiInitializeFvSecurity();
  ASSERT_EFI_ERROR (Status);

  //
  // Allocate an initial buffer from heap for debugger use
  //
  DEBUG_CODE (
    BpeDsAllocation ();
  );

  //
  // Do any early platform specific initialisation
  //
  EarlyPlatformInit (PlatformType);

  //
  // This is a second path on entry, in recovery boot path the Stall PPI need to be memory-based
  // to improve recovery performance.
  //
  Status = PeiServicesFfsGetFileInfo (FileHandle, &FileInfo);
  ASSERT_EFI_ERROR (Status);
  //
  // The follow conditional check only works for memory-mapped FFS,
  // so we ASSERT that the file is really a MM FFS.
  //
  ASSERT (FileInfo.Buffer != NULL);
  if (!(((UINTN) FileInfo.Buffer <= (UINTN) PeiInitPlatform) &&  
        ((UINTN) PeiInitPlatform <= (UINTN) FileInfo.Buffer + FileInfo.BufferSize))) {
    //
    // Now that module in memory, update the 
    // PPI that describes the Stall to other modules
    //
    Status = PeiServicesLocatePpi (
               &gEfiPeiStallPpiGuid,
               0,
               &StallPeiPpiDescriptor,
               (VOID **) &StallPpi
               );

    if (!EFI_ERROR (Status)) {

      Status = PeiServicesReInstallPpi (
                 StallPeiPpiDescriptor,
                 &mPpiStall[0]
                 );
    } else {
      
      Status = PeiServicesInstallPpi (&mPpiStall[0]);
    }
    return Status;
  }

  //
  // Initialise System Phys
  //

  // Program USB Phy
  InitializeUSBPhy();

  //
  // Do platform specific logic to create a boot mode
  //
  Status = UpdateBootMode ((EFI_PEI_SERVICES**)PeiServices, &BootMode);
  ASSERT_EFI_ERROR (Status);
  
  //
  // Signal possible dependent modules that there has been a 
  // final boot mode determination
  //
  if (!EFI_ERROR(Status)) {
    Status = PeiServicesInstallPpi (&mPpiBootMode[0]);
    ASSERT_EFI_ERROR (Status);
  }

  if (BootMode != BOOT_ON_S3_RESUME) {    
    QNCClearSmiAndWake ();    
  }

  //
  // Create the platform Flash Map
  //
  Status = PeimInitializeFlashMap (FileHandle, PeiServices);
  ASSERT_EFI_ERROR (Status);

  Status = PeimInstallFlashMapPpi (FileHandle, PeiServices);
  ASSERT_EFI_ERROR (Status);

  DEBUG ((EFI_D_INFO, "MRC Entry\n"));
  MemoryInit ((EFI_PEI_SERVICES**)PeiServices);

  //
  // Do Early PCIe init if not GalileoFabE Platform.
  //
  if (PlatformType != GalileoFabE) {
    DEBUG ((EFI_D_INFO, "Early PCIe controller initialisation\n"));
    PlatformPciExpressEarlyInit (PlatformType);
  }

  DEBUG ((EFI_D_INFO, "Platform Erratas After MRC\n"));
  PlatformErratasPostMrc ();

  //
  // Now that all of the pre-permament memory activities have
  // been taken care of, post a call-back for the permament-memory
  // resident services, such as HOB construction.
  // PEI Core will switch stack after this PEIM exit.  After that the MTRR
  // can be set.
  //
  Status = PeiServicesNotifyPpi (&mMemoryDiscoveredNotifyList[0]);
  ASSERT_EFI_ERROR (Status);
/*

  if (BootMode != BOOT_ON_S3_RESUME) {
    Status = PeiServicesNotifyPpi (mEndOfPeiSignalPpiNotifyList);
    ASSERT_EFI_ERROR (Status);
  }
*/  
  if (BootMode == BOOT_IN_RECOVERY_MODE) {
    PeiServicesRegisterForShadow (FileHandle);
  }

  return Status;
}

EFI_STATUS
EFIAPI
EndOfPeiSignalPpiNotifyCallback (
  IN EFI_PEI_SERVICES           **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR  *NotifyDescriptor,
  IN VOID                       *Ppi
  )
{
  EFI_STATUS                            Status;

  DEBUG ((EFI_D_INFO, "End of PEI Signal Callback\n"));

    //
  // Restore the flash region to be UC
  // for both normal boot as we build a Resource Hob to 
  // describe this region as UC to DXE core.
  //
  WriteBackInvalidateDataCacheRange (
    (VOID *) (UINTN) PcdGet32 (PcdFlashAreaBaseAddress),
    PcdGet32 (PcdFlashAreaSize)
  );    

  Status = MtrrSetMemoryAttribute (PcdGet32 (PcdFlashAreaBaseAddress), PcdGet32 (PcdFlashAreaSize), CacheUncacheable);
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

/**
  This function will initialize USB Phy registers associated with QuarkSouthCluster.

  @param  VOID                  No Argument

  @retval EFI_SUCCESS           All registers have been initialized
**/
VOID
EFIAPI
InitializeUSBPhy (
    VOID
   )
{
    UINT32 RegData32;

    /** In order to configure the PHY to use clk120 (ickusbcoreclk) as PLL reference clock
     *  and Port2 as a USB device port, the following sequence must be followed
     *
     **/

    // Sideband register write to USB AFE (Phy)
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_GLOBAL_PORT);
    RegData32 &= ~(BIT1);
    //
    // Sighting #4930631 PDNRESCFG [8:7] of USB2_GLOBAL_PORT = 11b.
    // For port 0 & 1 as host and port 2 as device.
    //
    RegData32 |= (BIT8 | BIT7);
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_GLOBAL_PORT, RegData32);

    //
    // Sighting #4930653 Required BIOS change on Disconnect vref to change to 600mV.
    //
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_COMPBG);
    RegData32 &= ~(BIT10 | BIT9 | BIT8 | BIT7);
    RegData32 |= (BIT10 | BIT7);
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_COMPBG, RegData32);

    // Sideband register write to USB AFE (Phy)
    // (pllbypass) to bypass/Disable PLL before switch
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2);
    RegData32 |= BIT29;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2, RegData32);

    // Sideband register write to USB AFE (Phy)
    // (coreclksel) to select 120MHz (ickusbcoreclk) clk source.
    // (Default 0 to select 96MHz (ickusbclk96_npad/ppad))
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1);
    RegData32 |= BIT1;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1, RegData32);

    // Sideband register write to USB AFE (Phy)
    // (divide by 8) to achieve internal 480MHz clock
    // for 120MHz input refclk.  (Default: 4'b1000 (divide by 10) for 96MHz)
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1);
    RegData32 &= ~(BIT5 | BIT4 | BIT3);
    RegData32 |= BIT6;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL1, RegData32);

    // Sideband register write to USB AFE (Phy)
    // Clear (pllbypass)
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2);
    RegData32 &= ~BIT29;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2, RegData32);

    // Sideband register write to USB AFE (Phy)
    // Set (startlock) to force the PLL FSM to restart the lock
    // sequence due to input clock/freq switch.
    RegData32 = QNCAltPortRead (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2);
    RegData32 |= BIT24;
    QNCAltPortWrite (QUARK_SC_USB_AFE_SB_PORT_ID, USB2_PLL2, RegData32);

    // At this point the PLL FSM and COMP FSM will complete

}

/**
  This function provides early platform Thermal sensor initialisation.
**/
VOID
EFIAPI
EarlyPlatformThermalSensorInit (
  VOID
  )
{
  DEBUG ((EFI_D_INFO, "Early Platform Thermal Sensor Init\n"));

  //
  // Set Thermal sensor mode.
  //
  QNCThermalSensorSetRatiometricMode ();

  //
  // Enable RMU Thermal sensor with a Catastrophic Trip point.
  //
  QNCThermalSensorEnableWithCatastrophicTrip (PLATFORM_CATASTROPHIC_TRIP_CELSIUS);

  //
  // Lock all RMU Thermal sensor control & trip point registers.
  //
  QNCThermalSensorLockAllRegisters ();
}

/**
  Print early platform info messages includeing the Stage1 module that's
  running, MFH item list and platform data item list.
**/
VOID
EFIAPI
EarlyPlatformInfoMessages (
  VOID
  )
{
  DEBUG_CODE_BEGIN ();
  QUARK_EDKII_STAGE1_HEADER       *Edk2ImageHeader;
  MFH_LIB_FINDCONTEXT               MfhFindContext;
  MFH_FLASH_ITEM                    *FlashItem;
  PDAT_ITEM                         *Item;
  PDAT_LIB_FINDCONTEXT              PDatFindContext;
  CHAR8                             Desc[PDAT_ITEM_DESC_LENGTH+1];

  //
  // Find which 'Stage1' image we are running and print the details
  //
  Edk2ImageHeader = (QUARK_EDKII_STAGE1_HEADER *) (FixedPcdGet32 (PcdEsramStage1Base) + FixedPcdGet32 (PcdFvSecurityHeaderSize));
  DEBUG ((EFI_D_INFO, "\n************************************************************\n"));

  if(FeaturePcdGet (PcdEnableSecureLock)) {
    DEBUG ((EFI_D_INFO, "****  Quark EDKII SECURE LOCKDOWN ENABLED              ****\n"));
  } else {          
    DEBUG ((EFI_D_INFO, "****  Quark EDKII SECURE LOCKDOWN DISABLED            ****\n"));
  }

  switch ((UINT8)Edk2ImageHeader->ImageIndex & QUARK_STAGE1_IMAGE_TYPE_MASK) {
    case QUARK_STAGE1_BOOT_IMAGE_TYPE:
      DEBUG ((EFI_D_INFO, "****  Quark EDKII Stage 1 Boot Image %d                ****\n", ((UINT8)Edk2ImageHeader->ImageIndex & ~(QUARK_STAGE1_IMAGE_TYPE_MASK))));
      break;

    case QUARK_STAGE1_RECOVERY_IMAGE_TYPE:
      DEBUG ((EFI_D_INFO, "****  Quark EDKII Stage 1 Recovery Image %d            ****\n", ((UINT8)Edk2ImageHeader->ImageIndex & ~(QUARK_STAGE1_IMAGE_TYPE_MASK))));
      break;

    default:
      DEBUG ((EFI_D_INFO, "****  Quark EDKII Unknown Stage 1 Image !!!!           ****\n"));
      break;
  }
  DEBUG (
    (EFI_D_INFO,
    "****  Quark EDKII Stage 2 Image 0x%08X:0x%08X ****\n" ,
    (UINTN) PcdGet32 (PcdFlashFvMainBase),
    (UINTN) PcdGet32 (PcdFlashFvMainSize)
    ));

  DEBUG (
    (EFI_D_INFO,
    "****  Quark EDKII Payload Image 0x%08X:0x%08X ****\n" ,
    (UINTN) PcdGet32 (PcdFlashFvPayloadBase),
    (UINTN) PcdGet32 (PcdFlashFvPayloadSize)
    ));

  DEBUG ((EFI_D_INFO, "************************************************************\n\n"));

  DEBUG ((EFI_D_INFO, "MFH Flash Item List:\n"));
  FlashItem = MfhLibFindFirstWithFilter (
                MFH_FIND_ANY_FIT_FILTER,
                FALSE,
                &MfhFindContext
                );
  while (FlashItem != NULL) {
    DEBUG ((EFI_D_INFO, "****  Quark 0x%08X:0x%08X %s ****\n", FlashItem->FlashAddress, FlashItem->LengthBytes, MfhLibFlashItemTypePrintString(FlashItem->Type)));
    FlashItem = MfhLibFindNextWithFilter (
                  MFH_FIND_ANY_FIT_FILTER,
                  &MfhFindContext
                  );
  }
  DEBUG ((EFI_D_INFO, "MFH Boot Priority List:\n"));
  FlashItem = MfhLibFindFirstWithFilter (
                MFH_FIND_ALL_STAGE1_FILTER,
                TRUE,
                &MfhFindContext
                );
  while (FlashItem != NULL) {
    DEBUG ((EFI_D_INFO, "****  Quark 0x%08X:0x%08X %s ****\n", FlashItem->FlashAddress, FlashItem->LengthBytes, MfhLibFlashItemTypePrintString(FlashItem->Type)));
    FlashItem = MfhLibFindNextWithFilter (
                  MFH_FIND_ALL_STAGE1_FILTER,
                  &MfhFindContext
                  );
  }

  DEBUG ((EFI_D_INFO, "\nPlatform Data Item List in System Area:\n"));
  Item = PDatLibFindFirstWithFilter (NULL, PDAT_FIND_ANY_ITEM_FILTER, &PDatFindContext, NULL);
  if (Item != NULL) {
    Desc[PDAT_ITEM_DESC_LENGTH] = 0;
    do {
      CopyMem (Desc, Item->Header.Description, PDAT_ITEM_DESC_LENGTH);
      DEBUG ((EFI_D_INFO, "****  Quark Data Id:Len = 0x%04X:0x%04X Desc = %a Ver=%04x ****\n", Item->Header.Identifier, Item->Header.Length, Desc, (UINTN) Item->Header.Version));
      Item = PDatLibFindNextWithFilter(PDAT_FIND_ANY_ITEM_FILTER, &PDatFindContext, NULL);
    } while (Item != NULL);
  }
  DEBUG ((EFI_D_INFO, "*************************************************\n\n"));

  DEBUG_CODE_END ();
}

/**
  Check if system reset due to error condition.

  @param  ClearErrorBits  If TRUE clear error flags and value bits.

  @retval TRUE  if system reset due to error condition.
  @retval FALSE if NO reset error conditions.
**/
BOOLEAN
CheckForResetDueToErrors (
  IN BOOLEAN                              ClearErrorBits
  )
{
  UINT32                            RegValue;
  BOOLEAN                           ResetDueToError;

  ResetDueToError = FALSE;

  //
  // Check if RMU reset system due to access violations.
  // RMU updates a SOC Unit register before reseting the system.
  //
  RegValue = QNCAltPortRead (QUARK_SCSS_SOC_UNIT_SB_PORT_ID, QUARK_SCSS_SOC_UNIT_CFG_STICKY_RW);
  if ((RegValue & B_CFG_STICKY_RW_VIOLATION) != 0) {
    ResetDueToError = TRUE;

    DEBUG (
      (EFI_D_ERROR,
      "\nReset due to access violation: %s %s %s %s\n",
      ((RegValue & B_CFG_STICKY_RW_IMR_VIOLATION) != 0) ? L"'IMR'" : L".",
      ((RegValue & B_CFG_STICKY_RW_DECC_VIOLATION) != 0) ? L"'DECC'" : L".",
      ((RegValue & B_CFG_STICKY_RW_SMM_VIOLATION) != 0) ? L"'SMM'" : L".",
      ((RegValue & B_CFG_STICKY_RW_HMB_VIOLATION) != 0) ? L"'HMB'" : L"."
      ));

    //
    // Clear error bits.
    //
    if (ClearErrorBits) {
      RegValue &= ~(B_CFG_STICKY_RW_VIOLATION);
      QNCAltPortWrite (QUARK_SCSS_SOC_UNIT_SB_PORT_ID, QUARK_SCSS_SOC_UNIT_CFG_STICKY_RW, RegValue);
    }
  }

  return ResetDueToError;
}

/**
  This function provides early platform initialisation.

  @param  PlatformType  Platform type to init.

**/
VOID
EFIAPI
EarlyPlatformInit (
  IN CONST EFI_PLATFORM_TYPE              PlatformType
  )
{

  //
  // Check if system reset due to error condition.
  //
  if (CheckForResetDueToErrors (TRUE)) {
    if(FeaturePcdGet (WaitIfResetDueToError)) {
      DEBUG ((EFI_D_ERROR, "Press any key to continue.\n"));
      PlatformDebugPortGetChar8 ();
    }
  }

  //
  // Display platform info messages.
  //
  EarlyPlatformInfoMessages ();

  //
  // Early Gpio Init.
  //
  EarlyPlatformGpioInit (PlatformType);

  //
  // Early platform GPIO manipulation depending on GPIOs
  // setup by EarlyPlatformGpioInit.
  //
  EarlyPlatformGpioManipulation (PlatformType);

  //
  // Early Thermal Sensor Init.
  //
  EarlyPlatformThermalSensorInit ();

  //
  // Init Redirect PEI services.
  //
  RedirectServicesInit ();

}

/**
  This function provides early platform GPIO initialisation.

  @param  PlatformType  Platform type for GPIO init.

**/
VOID
EFIAPI
EarlyPlatformGpioInit (
  IN CONST EFI_PLATFORM_TYPE              PlatformType
  )
{
  BOARD_LEGACY_GPIO_CONFIG          *LegacyGpioConfig;
  UINT32                            NewValue;
  UINT32                            GpioBaseAddress;

  //
  // Assert if platform type outside table range.
  //
  ASSERT ((UINTN) PlatformType < mBoardLegacyGpioConfigTableLen);

  LegacyGpioConfig = &mBoardLegacyGpioConfigTable[(UINTN) PlatformType];
  DEBUG ((EFI_D_INFO, "EarlyPlatformGpioInit for PlatType=0x%02x\n", (UINTN) PlatformType));

  GpioBaseAddress = (UINT32)PcdGet16 (PcdGbaIoBaseAddress);

  NewValue     = 0x0;
  //
  // Program QNC GPIO Registers.
  //
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGEN_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGEN_CORE_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGIO_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellIoSelect;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGIO_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGLVL_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellLvlForInputOrOutput;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGLVL_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGTPE_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellTriggerPositiveEdge;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGTPE_CORE_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGTNE_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellTriggerNegativeEdge;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGTNE_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGGPE_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellGPEEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGGPE_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGSMI_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellSMIEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGSMI_CORE_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CGTS_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellTriggerStatus;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CGTS_CORE_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_CNMIEN_CORE_WELL) & 0xFFFFFFFC) | LegacyGpioConfig->CoreWellNMIEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_CNMIEN_CORE_WELL, NewValue);

  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGEN_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGEN_RESUME_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGIO_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellIoSelect;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGIO_RESUME_WELL, NewValue) ;
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGLVL_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellLvlForInputOrOutput;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGLVL_RESUME_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGTPE_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellTriggerPositiveEdge;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGTPE_RESUME_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGTNE_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellTriggerNegativeEdge;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGTNE_RESUME_WELL, NewValue) ;
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGGPE_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellGPEEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGGPE_RESUME_WELL, NewValue);
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGSMI_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellSMIEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGSMI_RESUME_WELL, NewValue );
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RGTS_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellTriggerStatus;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RGTS_RESUME_WELL, NewValue) ;
  NewValue = (IoRead32 (GpioBaseAddress + R_QNC_GPIO_RNMIEN_RESUME_WELL) & 0xFFFFFFC0) | LegacyGpioConfig->ResumeWellNMIEnable;
  IoWrite32 (GpioBaseAddress + R_QNC_GPIO_RNMIEN_RESUME_WELL, NewValue);
}

/**
  Performs any early platform specific GPIO manipulation.

  @param  PlatformType  Platform type GPIO manipulation.

**/
VOID
EFIAPI
EarlyPlatformGpioManipulation (
  IN CONST EFI_PLATFORM_TYPE              PlatformType
  )
{
  if (PlatformType == CrossHill) {

    //
    // Pull TPM reset low for 80us (equivalent to cold reset, Table 39
    // Infineon SLB9645 Databook), then pull TPM reset high and wait for
    // 150ms to give time for TPM to stabilise (Section 4.7.1 Infineon
    // SLB9645 Databook states TPM is ready to receive command after 30ms
    // but section 4.7 states some TPM commands may take longer to execute
    // upto 150ms after test).
    //

    PlatformLegacyGpioSetLevel (
      R_QNC_GPIO_RGLVL_RESUME_WELL,
      PLATFORM_RESUMEWELL_TPM_RST_GPIO,
      FALSE
      );
    MicroSecondDelay (80);

    PlatformLegacyGpioSetLevel (
      R_QNC_GPIO_RGLVL_RESUME_WELL,
      PLATFORM_RESUMEWELL_TPM_RST_GPIO,
      TRUE
      );
    MicroSecondDelay (150000);
  }

}

