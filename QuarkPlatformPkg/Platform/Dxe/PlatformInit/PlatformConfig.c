/*++

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


Module Name:

  PlatformConfig.c

Abstract:

    Essential platform configuration.

Revision History

--*/

#include "PlatformInitDxe.h"

//
// The protocols, PPI and GUID defintions for this module
//

//
// The Library classes this module consumes
//

//
// RTC:28208 - System hang/crash when entering probe mode(ITP) when relocating SMBASE
//             Workaround to make default SMRAM UnCachable
//
#define SMM_DEFAULT_SMBASE                  0x30000     // Default SMBASE address
#define SMM_DEFAULT_SMBASE_SIZE_BYTES       0x10000     // Size in bytes of default SMRAM

BOOLEAN                       mMemCfgDone = FALSE;
BOOLEAN                       mPciCfgDone = FALSE;
BOARD_GPIO_CONTROLLER_CONFIG  mBoardGpioControllerConfigTable[]  = { PLATFORM_GPIO_CONTROLLER_CONFIG_DEFINITION };
UINTN                         mBoardGpioControllerConfigTableLen = (sizeof(mBoardGpioControllerConfigTable) / sizeof(BOARD_GPIO_CONTROLLER_CONFIG));
UINT8                         ChipsetDefaultMac [6] = {0xff,0xff,0xff,0xff,0xff,0xff};

VOID
EFIAPI
SetLanControllerMacAddr (
  IN CONST UINT8                          Bus,
  IN CONST UINT8                          Device,
  IN CONST UINT8                          Func,
  IN CONST UINT8                          *MacAddr
  )
/*++

Routine Description:

  Set Mac address on chipset ethernet device.

Arguments:
  Bus         - PCI Bus number of chipset ethernet device.
  Device      - PCI Device number of chipset ethernet device.
  Func        - PCI Function number of chipset ethernet device.
  MacAddr     - MAC Address to set.

Returns:
  None.

--*/
{
  UINT32                            Data32;
  UINT8                             PciCmd;
  UINT8                             Value8;
  UINT16                            PciVid;
  UINT16                            PciDid;
  UINT32                            Bar0;
  UINT32                            Addr;
  UINT32                            MacVer;
  volatile UINT8                    *Wrote;

  PciVid = IohMmPci16(0, Bus, Device, Func, PCI_REG_VID);
  PciDid = IohMmPci16(0, Bus, Device, Func, PCI_REG_DID);
  //
  // Read PCICMD.  Bus=0, Dev=0, Func=0, Reg=0x4
  //
  PciCmd = IohMmPci8(0, Bus, Device, Func, PCI_REG_PCICMD);

  if((PciVid == V_IOH_MAC_VENDOR_ID) && (PciDid == V_IOH_MAC_DEVICE_ID)) {
    //
    // Enable MMIO Space(Bit1).
    //
    Value8 = PciCmd | B_IOH_MAC_COMMAND_MSE;
    IohMmPci8(0, Bus, Device, Func, PCI_REG_PCICMD) = Value8;

    //
    // Read BAR0.  Bus=0, Dev=0, Func=0, Reg=0x10
    //
    Bar0 = IohMmPci32(0, Bus, Device, Func, R_IOH_MAC_MEMBAR) & B_IOH_MAC_MEMBAR_ADDRESS_MASK;

    Addr =  Bar0 + R_IOH_MAC_GMAC_REG_8;
    MacVer = *((volatile UINT32 *) (UINTN)(Addr));

    DEBUG ((EFI_D_INFO, "Ioh MAC [B:%d, D:%d, F:%d] VER:%04x ADDR:",
      (UINTN) Bus,
      (UINTN) Device,
      (UINTN) Func,
      (UINTN) MacVer
      ));

    //
    // Set MAC Address0 Low Register (GMAC_REG_17) ADDRLO bits.
    //
    Addr =  Bar0 + R_IOH_MAC_GMAC_REG_17;
    Data32 = *((UINT32 *) (UINTN)(&MacAddr[0]));
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;
    Wrote = (volatile UINT8 *) (UINTN)(Addr);
    DEBUG ((EFI_D_INFO, "%02x-%02x-%02x-%02x-",
      (UINTN) Wrote[0],
      (UINTN) Wrote[1],
      (UINTN) Wrote[2],
      (UINTN) Wrote[3]
      ));

    //
    // Set MAC Address0 High Register (GMAC_REG_16) ADDRHI bits
    // and Address Enable (AE) bit.
    //
    Addr =  Bar0 + R_IOH_MAC_GMAC_REG_16;
    Data32 = 
      ((UINT32) MacAddr[4]) |
      (((UINT32)MacAddr[5]) << 8) |
      B_IOH_MAC_AE;
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;
    Wrote = (volatile UINT8 *) (UINTN)(Addr);

    DEBUG ((EFI_D_INFO, "%02x-%02x\n", (UINTN) Wrote[0], (UINTN) Wrote[1]));

    //
    // Return Cmd register to initial value.
    //
    IohMmPci8(0, Bus, Device, Func, PCI_REG_PCICMD) = PciCmd;

  }
}

EFI_STATUS
EFIAPI
PlatformPcal9555Config (
  IN CONST EFI_PLATFORM_TYPE              PlatformType
  )
/*++

Routine Description:

Arguments:
  PlatformType - Set PCAL9555 IO Expander config for this platform.

Returns:
  EFI_STATUS

--*/
{
  EFI_STATUS                        Status;
  UINTN                             WriteLength;
  UINT8                             Data[3];
  EFI_I2C_DEVICE_ADDRESS            I2CDeviceAddr;
  EFI_I2C_ADDR_MODE                 I2CAddrMode;

  if (PlatformType != GalileoFabE) {
    return EFI_SUCCESS; // No error if plaform has no Pcal9555 IO Expander.
  }

  I2CDeviceAddr.I2CDeviceAddress = PCAL9555_GALILEO_GEN2_7BIT_SLAVE_ADDR;
  I2CAddrMode = EfiI2CSevenBitAddrMode;

  WriteLength = 3;
  Data[0] = PCAL9555_REG_CFG_PORT0; // Write to both cfg registers.
  Data[1] = PCAL9555_GALILEO_GEN2_PORT0_CFG;
  Data[2] = PCAL9555_GALILEO_GEN2_PORT1_CFG;

  Status = mI2cBus->WriteMultipleByte (
                      mI2cBus,
                      I2CDeviceAddr,
                      I2CAddrMode,
                      &WriteLength,
                      &Data
                      );
  ASSERT_EFI_ERROR (Status);

  WriteLength = 3;
  Data[0] = PCAL9555_REG_OUT_PORT0; // Write to both output registers.
  Data[1] = PCAL9555_GALILEO_GEN2_PORT0_DEFAULT_OUT;
  Data[2] = PCAL9555_GALILEO_GEN2_PORT1_DEFAULT_OUT;

  Status = mI2cBus->WriteMultipleByte (
                      mI2cBus,
                      I2CDeviceAddr,
                      I2CAddrMode,
                      &WriteLength,
                      &Data
                      );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}

VOID
EFIAPI
GpioControllerConfig (
  VOID
  )
/*++

Routine Description:

  Perform Gpio controller config.

Arguments:
  None.

Returns:
  None.

--*/
{
  UINT32                            IohGpioBase;
  UINT32                            Data32;
  UINT8                             Value8;
  UINT8                             PciCmd;
  UINT16                            PciVid;
  UINT16                            PciDid;
  UINT32                            Addr;
  BOARD_GPIO_CONTROLLER_CONFIG      *GpioConfig;
  UINT8                             Bus;
  UINT8                             Device;
  UINT8                             Func;
  EFI_PLATFORM_TYPE_PROTOCOL        *PlatformType;

  PlatformType = &mPrivatePlatformData.PlatformType;

  Bus = IOH_I2C_GPIO_BUS_NUMBER;
  Device = IOH_I2C_GPIO_DEVICE_NUMBER;
  Func = IOH_I2C_GPIO_FUNCTION_NUMBER;

  PciVid = IohMmPci16(0, Bus, Device, Func, PCI_REG_VID);
  PciDid = IohMmPci16(0, Bus, Device, Func, PCI_REG_DID);

  if((PciVid == V_IOH_I2C_GPIO_VENDOR_ID) && (PciDid == V_IOH_I2C_GPIO_DEVICE_ID)) {
    //
    // Read PCICMD.  Bus=0, Dev=0, Func=0, Reg=0x4
    //
    PciCmd = IohMmPci8(0, Bus, Device, Func, PCI_REG_PCICMD);

    //
    // Enable Bus Master(Bit2), MMIO Space(Bit1) & I/O Space(Bit0)
    //
    Value8 = PciCmd | 0x7;
    IohMmPci8(0, Bus, Device, Func, PCI_REG_PCICMD) = Value8;

    //
    // Read MEM_BASE.  Bus=0, Dev=0, Func=0, Reg=0x14
    //
    IohGpioBase = IohMmPci32(0, Bus, Device, Func, R_IOH_GPIO_MEMBAR);

    ASSERT ((UINTN) PlatformType->Type < mBoardGpioControllerConfigTableLen);
    GpioConfig = &mBoardGpioControllerConfigTable[(UINTN) PlatformType->Type];
    DEBUG ((EFI_D_INFO, "Ioh Gpio Controller Init for PlatType=0x%02x\n", (UINTN) PlatformType->Type));

    //
    // IEN- Interrupt Enable Register
    //
    Addr =  IohGpioBase + GPIO_INTEN;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->IntEn & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // ISTATUS- Interrupt Status Register
    //
    Addr =  IohGpioBase + GPIO_INTSTATUS;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // GPIO SWPORTA Direction Register - GPIO_SWPORTA_DR
    //
    Addr =  IohGpioBase + GPIO_SWPORTA_DR;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->PortADR & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // GPIO SWPORTA Data Direction Register - GPIO_SWPORTA_DDR - default input
    //
    Addr =  IohGpioBase + GPIO_SWPORTA_DDR;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->PortADir & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // Interrupt Mask Register - GPIO_INTMASK - default interrupts unmasked
    //
    Addr =  IohGpioBase + GPIO_INTMASK;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->IntMask & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // Interrupt Level Type Register - GPIO_INTTYPE_LEVEL - default is level sensitive
    //
    Addr =  IohGpioBase + GPIO_INTTYPE_LEVEL;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->IntType & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // Interrupt Polarity Type Register - GPIO_INT_POLARITY - default is active low
    //
    Addr =  IohGpioBase + GPIO_INT_POLARITY;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->IntPolarity & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // Interrupt Debounce Type Register - GPIO_DEBOUNCE - default no debounce
    //
    Addr =  IohGpioBase + GPIO_DEBOUNCE;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->Debounce & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

    //
    // Interrupt Clock Synchronisation Register - GPIO_LS_SYNC - default no sync with pclk_intr(APB bus clk)
    //
    Addr =  IohGpioBase + GPIO_LS_SYNC;
    Data32 = *((volatile UINT32 *) (UINTN)(Addr)) & 0xFFFFFF00; // Keep reserved bits [31:8]
    Data32 |= (GpioConfig->LsSync & 0x000FFFFF);
    *((volatile UINT32 *) (UINTN)(Addr)) = Data32;

  }
}

VOID
EFIAPI
PlatformResetDevices (
  VOID
  )
/*++

Routine Description:

  Performs any platform specific device resets 

Arguments:
  None.

Returns:
  None.

--*/
{
  UINT32                            IohGpioBase;
  UINT32                            Data32;
  UINT8                             Value8;
  UINT8                             PciCmd;
  UINT16                            PciVid;
  UINT16                            PciDid;
  UINT32                            Addr;
  UINT8                             Bus;
  UINT8                             Device;
  UINT8                             Func;
  EFI_PLATFORM_TYPE_PROTOCOL        *PlatformType;

  PlatformType = &mPrivatePlatformData.PlatformType;

  if(PlatformType->Type == (EFI_PLATFORM_TYPE) Galileo) {

    DEBUG ((EFI_D_INFO, "Resetting Cypress Expander\n"));

    Bus = IOH_I2C_GPIO_BUS_NUMBER;
    Device = IOH_I2C_GPIO_DEVICE_NUMBER;
    Func = IOH_I2C_GPIO_FUNCTION_NUMBER;

    PciVid = IohMmPci16(0, Bus, Device, Func, PCI_REG_VID);
    PciDid = IohMmPci16(0, Bus, Device, Func, PCI_REG_DID);

    if((PciVid == V_IOH_I2C_GPIO_VENDOR_ID) && (PciDid == V_IOH_I2C_GPIO_DEVICE_ID)) {
        //
        // Read PCICMD.  Bus=0, Dev=0, Func=0, Reg=0x4
        //
        PciCmd = IohMmPci8(0, Bus, Device, Func, PCI_REG_PCICMD);

        //
        // Enable Bus Master(Bit2), MMIO Space(Bit1) & I/O Space(Bit0)
        //
        Value8 = PciCmd | 0x7;
        IohMmPci8(0, Bus, Device, Func, PCI_REG_PCICMD) = Value8;

        //
        // Read MEM_BASE.  Bus=0, Dev=0, Func=0, Reg=0x14
        //
        IohGpioBase = IohMmPci32(0, Bus, Device, Func, R_IOH_GPIO_MEMBAR);
        ASSERT (IohGpioBase != 0xFFFFFFFF);

        //
        // Reset Cypress Expander on Galileo Platform
        //
        Addr = IohGpioBase + GPIO_SWPORTA_DR;
        Data32 = *((volatile UINT32 *) (UINTN)(Addr)); 
        Data32 |= BIT4;                                 // Cypress Reset line controlled by GPIO<4>
        *((volatile UINT32 *) (UINTN)(Addr)) = Data32; 
 
        Data32 = *((volatile UINT32 *) (UINTN)(Addr)); 
        Data32 &= ~BIT4;                                // Cypress Reset line controlled by GPIO<4>
        *((volatile UINT32 *) (UINTN)(Addr)) = Data32; 
    }
  } 
}

VOID
EFIAPI
PlatformConfigOnPciEnumComplete (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
/*++

Routine Description:

  Function runs in PI-DXE to perform platform specific config when PCI enum
  is complete.

Arguments:
  Event       - The event that occured.
  Context     - For EFI compatiblity.  Not used.

Returns:
  None.

--*/
{
  EFI_STATUS                        Status;
  BOOLEAN                           SetMacAddr;
  EFI_PLATFORM_TYPE_PROTOCOL        *PlatformType;
  VOID                              *PciEnumProt = NULL;

  PlatformType = &mPrivatePlatformData.PlatformType;

  Status = gBS->LocateProtocol (&gEfiPciEnumerationCompleteProtocolGuid, NULL, &PciEnumProt);
  if (Status != EFI_SUCCESS){
    DEBUG ((DEBUG_INFO, "gEfiPciEnumerationCompleteProtocolGuid triggered but not valid.\n"));
    return;
  }
  if (mPciCfgDone) {
    DEBUG ((DEBUG_INFO, "Platform DXE Pci config already done.\n"));
    return;
  }

  GpioControllerConfig ();
  PlatformResetDevices ();

  //
  // Set chipset MAC0 address if configured.
  //
  SetMacAddr =
    (CompareMem (ChipsetDefaultMac, PlatformType->SysData.IohMac0Address, sizeof (ChipsetDefaultMac))) != 0;
  if (SetMacAddr) {
    if ((*(PlatformType->SysData.IohMac0Address) & BIT0) != 0) {
      DEBUG ((EFI_D_ERROR, "HALT: Multicast Mac Address configured for Ioh MAC [B:%d, D:%d, F:%d]\n",
        (UINTN) IOH_MAC0_BUS_NUMBER,
        (UINTN) IOH_MAC0_DEVICE_NUMBER,
        (UINTN) IOH_MAC0_FUNCTION_NUMBER
        ));
      ASSERT (FALSE);
    } else {
      SetLanControllerMacAddr (
        IOH_MAC0_BUS_NUMBER,
        IOH_MAC0_DEVICE_NUMBER,
        IOH_MAC0_FUNCTION_NUMBER,
        PlatformType->SysData.IohMac0Address
        );
    }
  } else {
    DEBUG ((EFI_D_WARN, "WARNING: Ioh MAC [B:%d, D:%d, F:%d] NO HW ADDR CONFIGURED!!!\n",
      (UINTN) IOH_MAC0_BUS_NUMBER,
      (UINTN) IOH_MAC0_DEVICE_NUMBER,
      (UINTN) IOH_MAC0_FUNCTION_NUMBER
      ));
  }

  //
  // Set chipset MAC1 address if configured.
  //
  SetMacAddr =
    (CompareMem (ChipsetDefaultMac, PlatformType->SysData.IohMac1Address, sizeof (ChipsetDefaultMac))) != 0;
  if (SetMacAddr) {
    if ((*(PlatformType->SysData.IohMac1Address) & BIT0) != 0) {
      DEBUG ((EFI_D_ERROR, "HALT: Multicast Mac Address configured for Ioh MAC [B:%d, D:%d, F:%d]\n",
        (UINTN) IOH_MAC1_BUS_NUMBER,
        (UINTN) IOH_MAC1_DEVICE_NUMBER,
        (UINTN) IOH_MAC1_FUNCTION_NUMBER
        ));
      ASSERT (FALSE);
    } else {
        SetLanControllerMacAddr (
          IOH_MAC1_BUS_NUMBER,
          IOH_MAC1_DEVICE_NUMBER,
          IOH_MAC1_FUNCTION_NUMBER,
          PlatformType->SysData.IohMac1Address
          );
    }
  } else {
    DEBUG ((EFI_D_WARN, "WARNING: Ioh MAC [B:%d, D:%d, F:%d] NO HW ADDR CONFIGURED!!!\n",
      (UINTN) IOH_MAC1_BUS_NUMBER,
      (UINTN) IOH_MAC1_DEVICE_NUMBER,
      (UINTN) IOH_MAC1_FUNCTION_NUMBER
      ));
  }
  mPciCfgDone = TRUE;
}

VOID
EFIAPI
PlatformConfigOnSmmConfigurationProtocol (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
/*++

Routine Description:

  Function runs in PI-DXE to perform platform specific config when
  SmmConfigurationProtocol is installed.

Arguments:
  Event       - The event that occured.
  Context     - For EFI compatiblity.  Not used.

Returns:
  None.
--*/

{
  EFI_STATUS            Status;
  UINT32                NewValue;
  UINT64                BaseAddress;
  UINT64                SmramLength;
  EFI_CPU_ARCH_PROTOCOL *CpuArchProtocol;   // RTC:28208 - System hang/crash when entering probe mode(ITP) when relocating SMBASE
  VOID                  *SmmCfgProt;

  Status = gBS->LocateProtocol (&gEfiSmmConfigurationProtocolGuid, NULL, &SmmCfgProt);
  if (Status != EFI_SUCCESS){
    DEBUG ((DEBUG_INFO, "gEfiSmmConfigurationProtocolGuid triggered but not valid.\n"));
    return;
  }
  if (mMemCfgDone) {
    DEBUG ((DEBUG_INFO, "Platform DXE Mem config already done.\n"));
    return;
  }

  //
  // Disable eSram block (this will also clear/zero eSRAM)
  // We only use eSRAM in the PEI phase. Disable now that we are in the DXE phase
  //
  NewValue = QNCPortRead (QUARK_NC_MEMORY_MANAGER_SB_PORT_ID, QUARK_NC_MEMORY_MANAGER_ESRAMPGCTRL_BLOCK);
  NewValue |= BLOCK_DISABLE_PG;
  QNCPortWrite (QUARK_NC_MEMORY_MANAGER_SB_PORT_ID, QUARK_NC_MEMORY_MANAGER_ESRAMPGCTRL_BLOCK, NewValue);

  //
  // Update HMBOUND to top of DDR3 memory and LOCK
  // We disabled eSRAM so now we move HMBOUND down to top of DDR3
  //
  QNCGetTSEGMemoryRange (&BaseAddress, &SmramLength);
  NewValue = (UINT32)(BaseAddress + SmramLength);
  DEBUG ((EFI_D_INFO,"Locking HMBOUND at: = 0x%8x\n",NewValue));
  QNCPortWrite (QUARK_NC_HOST_BRIDGE_SB_PORT_ID, QUARK_NC_HOST_BRIDGE_HMBOUND_REG, (NewValue | HMBOUND_LOCK));

  //
  // Lock IMR5 now that HMBOUND is locked (legacy S3 region)
  //
  NewValue = QNCPortRead (QUARK_NC_MEMORY_MANAGER_SB_PORT_ID, QUARK_NC_MEMORY_MANAGER_IMR5+QUARK_NC_MEMORY_MANAGER_IMRXL);
  NewValue |= IMR_LOCK;
  QNCPortWrite (QUARK_NC_MEMORY_MANAGER_SB_PORT_ID, QUARK_NC_MEMORY_MANAGER_IMR5+QUARK_NC_MEMORY_MANAGER_IMRXL, NewValue);

  //
  // Lock IMR6 now that HMBOUND is locked (ACPI Reclaim/ACPI/Runtime services/Reserved)
  //
  NewValue = QNCPortRead (QUARK_NC_MEMORY_MANAGER_SB_PORT_ID, QUARK_NC_MEMORY_MANAGER_IMR6+QUARK_NC_MEMORY_MANAGER_IMRXL);
  NewValue |= IMR_LOCK;
  QNCPortWrite (QUARK_NC_MEMORY_MANAGER_SB_PORT_ID, QUARK_NC_MEMORY_MANAGER_IMR6+QUARK_NC_MEMORY_MANAGER_IMRXL, NewValue);

  //
  // Disable IMR2 memory protection (RMU Main Binary)
  //
  QncImrWrite (
            QUARK_NC_MEMORY_MANAGER_IMR2,
            (UINT32)(IMRL_RESET & ~IMR_EN),
            (UINT32)IMRH_RESET,
            (UINT32)IMRX_ALL_ACCESS,
            (UINT32)IMRX_ALL_ACCESS
        );

  //
  // Disable IMR3 memory protection (Default SMRAM)
  //
  QncImrWrite (
            QUARK_NC_MEMORY_MANAGER_IMR3,
            (UINT32)(IMRL_RESET & ~IMR_EN),
            (UINT32)IMRH_RESET,
            (UINT32)IMRX_ALL_ACCESS,
            (UINT32)IMRX_ALL_ACCESS
        );

  //
  // Disable IMR4 memory protection (eSRAM).
  //
  QncImrWrite (
            QUARK_NC_MEMORY_MANAGER_IMR4,
            (UINT32)(IMRL_RESET & ~IMR_EN),
            (UINT32)IMRH_RESET,
            (UINT32)IMRX_ALL_ACCESS,
            (UINT32)IMRX_ALL_ACCESS
        );

  //
  // RTC:28208 - System hang/crash when entering probe mode(ITP) when relocating SMBASE
  //             Workaround to make default SMRAM UnCachable
  //
  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **) &CpuArchProtocol);
  ASSERT_EFI_ERROR (Status);

  CpuArchProtocol->SetMemoryAttributes (
                     CpuArchProtocol,
                     (EFI_PHYSICAL_ADDRESS) SMM_DEFAULT_SMBASE,
                     SMM_DEFAULT_SMBASE_SIZE_BYTES,
                     EFI_MEMORY_WB 
                     );

  mMemCfgDone = TRUE;
}

VOID
EFIAPI
PlatformConfigOnSpiReady (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
/*++

Routine Description:

  Function runs in PI-DXE to perform platform specific config when SPI
  interface is ready.

Arguments:
  Event       - The event that occured.
  Context     - For EFI compatiblity.  Not used.

Returns:
  None.

--*/
{
  EFI_STATUS                        Status;
  VOID                              *SpiReadyProt = NULL;

  Status = gBS->LocateProtocol (&gEfiSmmSpiReadyProtocolGuid, NULL, &SpiReadyProt);
  if (Status != EFI_SUCCESS){
    DEBUG ((DEBUG_INFO, "gEfiSmmSpiReadyProtocolGuid triggered but not valid.\n"));
    return;
  }

  //
  // Lock regions SPI flash.
  //
  PlatformFlashLockPolicy (FALSE);

}

EFI_STATUS
EFIAPI
CreateConfigEvents (
  VOID
  )
/*++

Routine Description:

Arguments:
  None

Returns:
  EFI_STATUS

--*/
{
  EFI_EVENT   EventSmmCfg;
  EFI_EVENT   EventPci;
  EFI_EVENT   EventSpiReady;
  VOID        *RegistrationSmmCfg;
  VOID        *RegistrationPci;
  VOID        *RegistrationSpiReady;

  //
  // Schedule callback for when SmmConfigurationProtocol installed.
  //
  EventSmmCfg = EfiCreateProtocolNotifyEvent (
                  &gEfiSmmConfigurationProtocolGuid,
                  TPL_CALLBACK,
                  PlatformConfigOnSmmConfigurationProtocol,
                  NULL,
                  &RegistrationSmmCfg
                  );
  ASSERT (EventSmmCfg != NULL);

  //
  // Schedule callback to setup IOH GPIO controller registers when PCI enum
  // complete (MEMBASE assigned).
  //
  EventPci = EfiCreateProtocolNotifyEvent (
               &gEfiPciEnumerationCompleteProtocolGuid,
               TPL_CALLBACK,
               PlatformConfigOnPciEnumComplete,
               NULL,
               &RegistrationPci
               );
  ASSERT (EventPci != NULL);

  //
  // Schedule callback to setup SPI Flash Policy when SPI interface ready.
  //
  EventSpiReady = EfiCreateProtocolNotifyEvent (
                    &gEfiSmmSpiReadyProtocolGuid,
                    TPL_CALLBACK,
                    PlatformConfigOnSpiReady,
                    NULL,
                    &RegistrationSpiReady
                    );
  ASSERT (EventSpiReady != NULL);
  return EFI_SUCCESS;
}
