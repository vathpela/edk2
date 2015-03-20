/** @file
  I2C DXE Driver for Quark I2C Controller.
  Follows I2C Controller setup instructions as detailed in
  Quark DataSheet (doc id: 329676) Section 19.1/19.1.3.

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

#include "I2C.h"

//
// Interface definition of I2C Host Controller Protocol.
//
EFI_I2C_HC_PROTOCOL mI2CbusHc = {
  I2CWriteByte,
  I2CReadByte,
  I2CWriteMultipleByte,
  I2CReadMultipleByte
};

//
// Handle to install I2C Controller protocol.
//

STATIC EFI_HANDLE mHandle = NULL;
STATIC UINT16     mSaveCmdReg;
STATIC UINT32     mSaveBar0Reg;

/**
  The Called on Protocol Service Entry.

  @return None.

**/
STATIC
VOID
ServiceEntry (
  VOID
  )
{
  mSaveBar0Reg = IohMmPci32 (0, I2C_Bus, I2C_Device, I2C_Func, PCI_BAR0);
  if ((mSaveBar0Reg & B_IOH_I2C_GPIO_MEMBAR_ADDR_MASK) == 0) {

    IohMmPci32(0, I2C_Bus, I2C_Device, I2C_Func, PCI_BAR0) =
      FixedPcdGet32 (PcdIohI2cMmioBase) & B_IOH_I2C_GPIO_MEMBAR_ADDR_MASK;

    //
    // also Save Cmd Register, Setup by InitializeInternal later during xfers.
    //
    mSaveCmdReg = IohMmPci16 (0, I2C_Bus, I2C_Device, I2C_Func, PCI_CMD);
  }
}

/**
  The Called on Protocol Service Entry.

  @return None.

**/
STATIC
VOID
ServiceExit (
  VOID
  )
{
  if ((mSaveBar0Reg & B_IOH_I2C_GPIO_MEMBAR_ADDR_MASK) == 0) {
    IohMmPci16 (0, I2C_Bus, I2C_Device, I2C_Func, PCI_CMD) = mSaveCmdReg;
    IohMmPci32 (0, I2C_Bus, I2C_Device, I2C_Func, PCI_BAR0) = mSaveBar0Reg;
  }
}


/**
  The GetI2CIoPortBaseAddress() function gets IO port base address of I2C Controller.

  Always reads PCI configuration space to get MMIO base address of I2C Controller.

  @return The IO port base address of I2C controller.

**/
UINTN
GetI2CIoPortBaseAddress (
  VOID
  )
{
  UINTN     I2CIoPortBaseAddress;

  //
  // Get I2C Memory Mapped registers base address.
  //
  I2CIoPortBaseAddress = IohMmPci32(0, I2C_Bus, I2C_Device, I2C_Func, PCI_BAR0);

  //
  // Make sure that the IO port base address has been properly set.
  //
  ASSERT (I2CIoPortBaseAddress != 0);
  ASSERT (I2CIoPortBaseAddress != 0xFF);

  return I2CIoPortBaseAddress;
}


/**
  The EnableI2CMmioSpace() function enables access to I2C MMIO space.

**/
VOID
EnableI2CMmioSpace (
  VOID
  )
{
  UINT8 PciCmd;

  //
  // Read PCICMD.  Bus=0, Dev=0, Func=0, Reg=0x4
  //
  PciCmd = IohMmPci8(0, I2C_Bus, I2C_Device, I2C_Func, PCI_REG_PCICMD);

  //
  // Enable Bus Master(Bit2), MMIO Space(Bit1) & I/O Space(Bit0)
  //
  PciCmd |= 0x7;
  IohMmPci8(0, I2C_Bus, I2C_Device, I2C_Func, PCI_REG_PCICMD) = PciCmd;

}


/**
  The DisableI2CController() functions disables I2C Controller.

  @retval EFI_SUCCESS           I2C Controller disabled successfully.

  @retval EFI_DEVICE_ERROR      I2C Controller could not be disabled.

**/
EFI_STATUS
DisableI2CController (
  )
{
  UINTN   I2CIoPortBaseAddress;
  UINT32  Addr;
  UINT32  Data;
  UINT8   PollCount;

  PollCount = 0;

  //
  // Get I2C Memory Mapped registers base address.
  //
  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Disable the I2C Controller by setting IC_ENABLE.ENABLE to zero
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_ENABLE;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= ~B_I2C_REG_ENABLE;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Read the IC_ENABLE_STATUS.IC_EN Bit to check if Controller is disabled
  //
  Data = 0xFF;
  Addr = I2CIoPortBaseAddress + I2C_REG_ENABLE_STATUS;
  Data = *((volatile UINT32 *) (UINTN)(Addr)) & I2C_REG_ENABLE_STATUS;

  if (Data == 0) {
    return EFI_SUCCESS;
  }

  //
  // Poll the IC_ENABLE_STATUS.IC_EN Bit to check if Controller is disabled, until timeout (TI2C_POLL*MAX_T_POLL_COUNT).
  //
PollIcEn:
  PollCount++;
  if (PollCount >= MAX_T_POLL_COUNT) {
    return EFI_DEVICE_ERROR;
  } else {
    gBS->Stall(TI2C_POLL);
    Data = *((volatile UINT32 *) (UINTN)(Addr));
    Data &= I2C_REG_ENABLE_STATUS;
    if (Data == 0) {
      return EFI_SUCCESS;
    } else {
        goto PollIcEn;
    }
  }

}

/**
  The EnableI2CController() function enables the I2C Controller.

**/
VOID
EnableI2CController (
  )
{
  UINTN   I2CIoPortBaseAddress;
  UINT32  Addr;
  UINT32  Data;

  //
  // Get I2C Memory Mapped registers base address.
  //
  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Enable the I2C Controller by setting IC_ENABLE.ENABLE to 1
  //    
  Addr = I2CIoPortBaseAddress + I2C_REG_ENABLE;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data |= B_I2C_REG_ENABLE;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

}

/**
  The WaitForStopDet() function waits until I2C STOP Condition occurs,
  indicating transfer completion.

**/
VOID
WaitForStopDet (
  )
{
  UINTN   I2CIoPortBaseAddress;
  UINT32  Addr;
  UINT32  Data;

  //
  // Get I2C Memory Mapped registers base address.
  //
  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Wait for STOP Detect.
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_RAW_INTR_STAT;

PollStopDet:
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= I2C_REG_RAW_INTR_STAT_STOP_DET;
  if (Data == 0) {
    gBS->Stall(TI2C_POLL);
    goto PollStopDet;
  }

}

/**

  The InitializeInternal() function initialises internal I2C Controller
  register values that are commonly required for I2C Write and Read transfers.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @retval EFI_SUCCESS           I2C Operation completed successfully.

  @retval EFI_DEVICE_ERROR      The request was not completed due to error
                                accessing the slave device.

**/
EFI_STATUS
InitializeInternal (
  IN  EFI_I2C_ADDR_MODE  AddrMode
  )
{
  UINTN       I2CIoPortBaseAddress;
  UINTN       Addr;
  UINT32      Data;
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;

  //
  // Enable access to I2C Controller MMIO space.
  //
  EnableI2CMmioSpace ();

  //
  // Disable I2C Controller initially
  //
  Status = DisableI2CController ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get I2C Memory Mapped registers base address.
  //
  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Clear START_DET
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_CLR_START_DET;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= ~B_I2C_REG_CLR_START_DET;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Clear STOP_DET
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_CLR_STOP_DET;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= ~B_I2C_REG_CLR_STOP_DET;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Set addressing mode to user defined (7 or 10 bit) and
  // speed mode to that defined by PCD (standard mode default).
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_CON;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  // Set Addressing Mode
  if (AddrMode == EfiI2CSevenBitAddrMode) {
    Data &= ~B_I2C_REG_CON_10BITADD_MASTER;
  } else {
    Data |= B_I2C_REG_CON_10BITADD_MASTER;
  }
  // Set Speed Mode
  Data &= ~B_I2C_REG_CON_SPEED;
  if (FeaturePcdGet (PcdI2CFastModeEnabled)) {
    Data |= BIT2;
  } else {
    Data |= BIT1;
  }
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  Data = *((volatile UINT32 *) (UINTN)(Addr));

  return Status;

}

/**

  The WriteByte() function provides a standard way to execute a
  standard single byte write to an IC2 device (without accessing
  sub-addresses), as defined in the I2C Specification.

  @param  I2CAddress      I2C Slave device address
  @param  Value           The 8-bit value to write.
  @param  Status          Return status for the executed command (EFI_SUCCESS or EFI_DEVICE_ERROR).
                          This is an optional parameter and may be NULL.

  @return Data written to I2C Slave device, as read from I2C Data CMD register.

**/
UINT8
EFIAPI
WriteByte (
  IN  UINTN          I2CAddress,
  IN  UINT8          Value,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINTN   I2CIoPortBaseAddress;
  UINTN   Addr;
  UINT32  Data;
  UINT8   ReturnData;

  //
  // Get I2C Memory Mapped registers base address
  //
  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Write to the IC_TAR register the address of the slave device to be addressed
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_TAR;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= ~B_I2C_REG_TAR;
  Data |= I2CAddress;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Enable the I2C Controller
  //
  EnableI2CController ();

  //
  // Write the data and transfer direction to the IC_DATA_CMD register.
  // Also specify that transfer should be terminated by STOP condition.
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_DATA_CMD;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= 0xFFFFFF00;
  Data |= (UINT8)Value;
  Data &= ~B_I2C_REG_DATA_CMD_RW;
  Data |= B_I2C_REG_DATA_CMD_STOP;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Wait for transfer completion.
  //
  WaitForStopDet ();

  //
  // Return write data byte from TX/RX buffer (IC_DATA_CMD[7:0]).
  // 
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= 0x000000FF;
  ReturnData = (UINT8)Data;

  return ReturnData;

}

/**

  The ReadByte() function provides a standard way to execute a
  standard single byte read to an IC2 device (without accessing
  sub-addresses), as defined in the I2C Specification.

  @param  I2CAddress      I2C Slave device address
  @param  Status          Return status for the executed command (EFI_SUCCESS or EFI_DEVICE_ERROR).
                          This is an optional parameter and may be NULL.

  @return Data read from the I2C Slave device.

**/ 
UINT8
EFIAPI
ReadByte (
  IN  UINTN          I2CAddress,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINTN   I2CIoPortBaseAddress;
  UINTN   Addr;
  UINT32  Data;
  UINT8   ReturnData;

  //
  // Get I2C Memory Mapped registers base address.
  //
  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Write to the IC_TAR register the address of the slave device to be addressed
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_TAR;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= ~B_I2C_REG_TAR;
  Data |= I2CAddress;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Enable the I2C Controller
  //
  EnableI2CController ();

  //
  // Write transfer direction to the IC_DATA_CMD register and
  // specify that transfer should be terminated by STOP condition.
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_DATA_CMD;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= 0xFFFFFF00;
  Data |= B_I2C_REG_DATA_CMD_RW;
  Data |= B_I2C_REG_DATA_CMD_STOP;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Wait for transfer completion
  //
  WaitForStopDet ();

  //
  // Obtain and return read data byte from RX buffer (IC_DATA_CMD[7:0]).
  //
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= 0x000000FF;
  ReturnData = (UINT8)Data;

  return ReturnData;

}

/**

  The WriteMultipleByte() function provides a standard way to execute
  multiple byte writes to an IC2 device (e.g. when accessing sub-addresses or
  when writing block of data), as defined in the I2C Specification.

  @param I2CAddress   The I2C slave address of the device
                      with which to communicate.

  @param Buffer       Contains the value of byte to be written to the
                      I2C slave device.

  @param Length       No. of bytes to be written.

  @param  Status      Return status for the executed command (EFI_SUCCESS or EFI_DEVICE_ERROR).
                      This is an optional parameter and may be NULL.

**/
VOID
EFIAPI
WriteMultipleByte (
  IN  UINTN          I2CAddress,
  IN  UINT8          *WriteBuffer,
  IN  UINTN          Length,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINTN   I2CIoPortBaseAddress;
  UINTN   Index;
  UINTN   Addr;
  UINT32  Data;

  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Write to the IC_TAR register the address of the slave device to be addressed
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_TAR;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= ~B_I2C_REG_TAR;
  Data |= I2CAddress;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Enable the I2C Controller
  //
  EnableI2CController ();

  //
  // Write the data and transfer direction to the IC_DATA_CMD register.
  // Also specify that transfer should be terminated by STOP condition.
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_DATA_CMD;
  for (Index = 0; Index < Length; Index++) {
    Data = *((volatile UINT32 *) (UINTN)(Addr));
    Data &= 0xFFFFFF00;
    Data |= (UINT8)WriteBuffer[Index];
    Data &= ~B_I2C_REG_DATA_CMD_RW;
    if (Index == (Length-1)) {
      Data |= B_I2C_REG_DATA_CMD_STOP;
    }
    *((volatile UINT32 *) (UINTN)(Addr)) = Data;
  }

  //
  // Wait for transfer completion
  //
  WaitForStopDet ();

}

/**

  The ReadMultipleByte() function provides a standard way to execute
  multiple byte writes to an IC2 device (e.g. when accessing sub-addresses or
  when reading block of data), as defined in the I2C Specification (I2C combined 
  write/read protocol).

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param Buffer       Contains the value of byte data written or read from the
                      I2C slave device.

  @param WriteLength  No. of bytes to be written. In this case data
                      written typically contains sub-address or sub-addresses
                      in Hi-Lo format, that need to be read (I2C combined 
                      write/read protocol).

  @param ReadLength   No. of bytes to be read. I

  @param ReadLength   No. of bytes to be read from I2C slave device. 
                      need to be read.

  @param Buffer       Contains the value of byte data read from the
                      I2C slave device.

  @param  Status      Return status for the executed command (EFI_SUCCESS or EFI_DEVICE_ERROR).
                      This is an optional parameter and may be NULL.

**/
VOID
EFIAPI
ReadMultipleByte (
  IN  UINTN          I2CAddress,
  IN  OUT UINT8      *Buffer,
  IN  UINTN          WriteLength,
  IN  UINTN          ReadLength,
  OUT RETURN_STATUS  *Status        OPTIONAL
  )
{
  UINTN   I2CIoPortBaseAddress;
  UINTN   Index;
  UINTN   Addr;
  UINT32  Data;
  UINT8   PollCount;

  I2CIoPortBaseAddress = GetI2CIoPortBaseAddress ();

  //
  // Write to the IC_TAR register the address of the slave device to be addressed
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_TAR;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  Data &= ~B_I2C_REG_TAR;
  Data |= I2CAddress;
  *((volatile UINT32 *) (UINTN)(Addr)) = Data;

  //
  // Enable the I2C Controller
  //
  EnableI2CController ();

  //
  // Write the data (sub-addresses) to the IC_DATA_CMD register.
  //
  Addr = I2CIoPortBaseAddress + I2C_REG_DATA_CMD;
  for (Index = 0; Index < WriteLength; Index++) {
    Data = *((volatile UINT32 *) (UINTN)(Addr));
    Data &= 0xFFFFFF00;
    Data |= (UINT8)Buffer[Index];
    Data &= ~B_I2C_REG_DATA_CMD_RW;
    *((volatile UINT32 *) (UINTN)(Addr)) = Data;
  }

  //
  // Issue Read Transfers for each byte (Restart issued when write/read bit changed).
  //
  for (Index = 0; Index < ReadLength; Index++) {
    Data = *((volatile UINT32 *) (UINTN)(Addr));
    Data |= B_I2C_REG_DATA_CMD_RW;
    // Issue a STOP for last read transfer.
    if (Index == (ReadLength-1)) {
      Data |= B_I2C_REG_DATA_CMD_STOP;
    }
    *((volatile UINT32 *) (UINTN)(Addr)) = Data;
  }
  
  //
  // Wait for STOP condition.
  //
  WaitForStopDet ();

  //
  // Poll Receive FIFO Buffer Level register until valid (upto MAX_T_POLL_COUNT times).
  //
  Data = 0;
  PollCount = 0;
  Addr = I2CIoPortBaseAddress + I2C_REG_RXFLR;
  Data = *((volatile UINT32 *) (UINTN)(Addr));
  while ((Data != ReadLength) && (PollCount < MAX_T_POLL_COUNT)) {
    gBS->Stall(TI2C_POLL);
	PollCount++;
    Data = *((volatile UINT32 *) (UINTN)(Addr));
  }
  
  //
  // If receive buffer valid output data, otherwise return device error.
  //
  if (PollCount == MAX_T_POLL_COUNT) {
    *Status = EFI_DEVICE_ERROR;
  } else {
	*Status = EFI_SUCCESS;
    Addr = I2CIoPortBaseAddress + I2C_REG_DATA_CMD;
    for (Index = 0; Index < ReadLength; Index++) {
      Data = *((volatile UINT32 *) (UINTN)(Addr));
      Data &= 0x000000FF;
      *(Buffer+Index) = (UINT8)Data;  
    }
  }

}


/**

  The I2CWriteByte() function is a wrapper function for the WriteByte function.
  Provides a standard way to execute a standard single byte write to an IC2 device 
  (without accessing sub-addresses), as defined in the I2C Specification.

  @param This         A pointer to the EFI_I2C_PROTOCOL instance.

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param Length       No. of bytes to be written.

  @param Buffer       Contains the value of byte data to execute to the
                      I2C slave device.


  @retval EFI_SUCCESS           I2C Operation completed successfully.

  @retval EFI_DEVICE_ERROR      The request was not completed due to error
                                accessing the slave device.

**/
EFI_STATUS
EFIAPI
I2CWriteByte (
  IN CONST  EFI_I2C_HC_PROTOCOL     *This,
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN UINTN                          *Length,
  IN OUT    VOID                    *Buffer
  )
{
  EFI_STATUS Status;
  UINTN      I2CAddress;

  ServiceEntry ();

  Status = EFI_SUCCESS;

  I2CAddress = SlaveAddress.I2CDeviceAddress;

  Status = InitializeInternal (AddrMode);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteByte (I2CAddress, *(UINT8 *) Buffer, &Status);

  ServiceExit ();
  return Status;
}

/**

  The I2CReadByte() function is a wrapper function for the ReadByte function.
  Provides a standard way to execute a standard single byte read to an IC2 device 
  (without accessing sub-addresses), as defined in the I2C Specification.

  @param This         A pointer to the EFI_I2C_PROTOCOL instance.

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param Length       No. of bytes to be read.

  @param Buffer       Contains the value of byte data read from the
                      I2C slave device.


  @retval EFI_SUCCESS           I2C Operation completed successfully.

  @retval EFI_DEVICE_ERROR      The request was not completed due to error
                                accessing the slave device.

**/
EFI_STATUS
EFIAPI
I2CReadByte (
  IN CONST  EFI_I2C_HC_PROTOCOL     *This,
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN UINTN                          *Length,
  IN OUT    VOID                    *Buffer
  )
{
  EFI_STATUS Status;
  UINTN      I2CAddress;

  ServiceEntry ();

  Status = EFI_SUCCESS;

  I2CAddress = SlaveAddress.I2CDeviceAddress;

  Status = InitializeInternal (AddrMode);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  *(UINT8 *) Buffer = ReadByte (I2CAddress, &Status);

  ServiceExit ();
  return Status;
}

/**

  The I2CWriteMultipleByte() function is a wrapper function for the WriteMultipleByte() function.
  Provides a standard way to execute multiple byte writes to an IC2 device 
  (e.g. when accessing sub-addresses or writing block of data), as defined 
  in the I2C Specification.

  @param This         A pointer to the EFI_I2C_PROTOCOL instance.

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param Length       No. of bytes to be written.

  @param Buffer       Contains the value of byte to be written to the
                      I2C slave device.


  @retval EFI_SUCCESS           I2C Operation completed successfully.

  @retval EFI_DEVICE_ERROR      The request was not completed due to error
                                accessing the slave device.

**/
EFI_STATUS
EFIAPI
I2CWriteMultipleByte (
  IN CONST  EFI_I2C_HC_PROTOCOL     *This,
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN UINTN                          *Length,
  IN OUT    VOID                    *Buffer
  )
{
  EFI_STATUS Status;
  UINTN      I2CAddress;

  ServiceEntry ();
  Status = EFI_SUCCESS;

  I2CAddress = SlaveAddress.I2CDeviceAddress;

  Status = InitializeInternal (AddrMode);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  WriteMultipleByte (I2CAddress, Buffer, (*Length), &Status);

  ServiceExit ();
  return Status;
}

/**

  The I2CReadMultipleByte() function is a wrapper function for the ReadMultipleByte() function.
  Provides a standard way to execute multiple byte writes to an IC2 device 
  (e.g. when accessing sub-addresses or when reading block of data), as defined 
  in the I2C Specification (I2C combined write/read protocol).

  @param This         A pointer to the EFI_I2C_PROTOCOL instance.

  @param SlaveAddress The I2C slave address of the device
                      with which to communicate.

  @param AddrMode     I2C Addressing Mode: 7-bit or 10-bit address.

  @param WriteLength  No. of bytes to be written. In this case data
                      written typically contains sub-address or sub-addresses
                      in Hi-Lo format, that need to be read (I2C combined 
                      write/read protocol).

  @param ReadLength   No. of bytes to be read from I2C slave device. 
                      need to be read.

  @param Buffer       Contains the value of byte data read from the
                      I2C slave device.


  @retval EFI_SUCCESS           I2C Operation completed successfully.

  @retval EFI_DEVICE_ERROR      The request was not completed due to error
                                accessing the slave device.

**/
EFI_STATUS
EFIAPI
I2CReadMultipleByte (
  IN CONST  EFI_I2C_HC_PROTOCOL     *This,
  IN        EFI_I2C_DEVICE_ADDRESS  SlaveAddress,
  IN        EFI_I2C_ADDR_MODE       AddrMode,
  IN UINTN                          *WriteLength,
  IN UINTN                          *ReadLength,
  IN OUT    VOID                    *Buffer
  )
{
  EFI_STATUS Status;
  UINTN      I2CAddress;

  ServiceEntry ();

  Status = EFI_SUCCESS;

  I2CAddress = SlaveAddress.I2CDeviceAddress;

  Status = InitializeInternal (AddrMode);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  ReadMultipleByte(I2CAddress, Buffer, (*WriteLength), (*ReadLength), &Status);

  ServiceExit ();
  return Status;
}

/**
  Entry point to the DXE Driver that produces the I2C Controller Protocol.

  @param  ImageHandle      ImageHandle of the loaded driver.
  @param  SystemTable      Pointer to the EFI System Table.

  @retval EFI_SUCCESS      The entry point of I2C DXE driver is executed successfully.
  @retval !EFI_SUCESS      Some error occurs in the entry point of I2C DXE driver.

**/
EFI_STATUS
EFIAPI
InitializeI2C (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS    Status;
  Status = EFI_SUCCESS;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mHandle,
                  &gEfiI2CHcProtocolGuid,
                  &mI2CbusHc,
                  NULL
                  );

  ASSERT_EFI_ERROR (Status);

  return Status;
}
