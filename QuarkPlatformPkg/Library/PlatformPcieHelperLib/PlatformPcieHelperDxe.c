/** @file

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

  PlatformPcieHelperDxe.c

Abstract:

  Implementation of Pci express helper routines for DXE enviroment.

--*/

#include <PiDxe.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/S3BootScriptLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/UefiLib.h>

#include <Protocol/PlatformType.h>
#include <Protocol/I2CHc.h>

#include "CommonHeader.h"

//
// Routines defined in other source modules of this component.
//

//
// Routines local to this component.
//

/**
  Set the level of Pcal9555 IO Expander GPIO high or low.

  No error returned. Function asserts if unable to address io expander.
  Expected to be used with on board devices.

  @param  I2cBus             I2c Host controller protocol.
  @param  Pcal9555SlaveAddr  I2c Slave address of Pcal9555 Io Expander.
  @param  GpioNum            Gpio to change values 0-7 for Port0 and 8-15
                             for Port1.
  @param  HighLevel          If TRUE set pin high else set pin low.

**/
STATIC
VOID
Pcal9555GpioSetLevel (
  IN EFI_I2C_HC_PROTOCOL                  *I2cBus,
  IN CONST UINTN                          Pcal9555SlaveAddr,
  IN CONST UINT16                         GpioNum,
  IN CONST BOOLEAN                        HighLevel
  )
{
  EFI_STATUS                        Status;
  UINTN                             ReadLength;
  UINTN                             WriteLength;
  UINT8                             Data[2];
  EFI_I2C_DEVICE_ADDRESS            I2cDeviceAddr;
  EFI_I2C_ADDR_MODE                 I2cAddrMode;
  UINT8                             *RegValuePtr;
  UINT8                             GpioNumMask;
  UINT8                             SubAddr;

  ASSERT (I2cBus != NULL);

  I2cDeviceAddr.I2CDeviceAddress = Pcal9555SlaveAddr;
  I2cAddrMode = EfiI2CSevenBitAddrMode;

  if (GpioNum < 8) {
    SubAddr = PCAL9555_REG_OUT_PORT0;
    GpioNumMask = (UINT8) (1 << GpioNum);
  } else {
    SubAddr = PCAL9555_REG_OUT_PORT0 + 1;
    GpioNumMask = (UINT8) (1 << (GpioNum - 8));
  }

  //
  // Output port value always at 2nd byte in Data variable.
  //
  RegValuePtr = &Data[1];

  //
  // On read entry sub address at 2nd byte, on read exit output
  // port value in 2nd byte.
  //
  Data[1] = SubAddr;
  WriteLength = 1;
  ReadLength = 1;
  Status = I2cBus->ReadMultipleByte (
                      I2cBus,
                      I2cDeviceAddr,
                      I2cAddrMode,
                      &WriteLength,
                      &ReadLength,
                      &Data[1]
                      );
  ASSERT_EFI_ERROR (Status);

  //
  // Adjust output port value given callers request.
  //
  if (HighLevel) {
    *RegValuePtr = *RegValuePtr | GpioNumMask;
  } else {
    *RegValuePtr = *RegValuePtr & ~(GpioNumMask);
  }

  //
  // Update register. Sub address at 1st byte, value at 2nd byte.
  //
  WriteLength = 2;
  Data[0] = SubAddr;
  Status = I2cBus->WriteMultipleByte (
                      I2cBus,
                      I2cDeviceAddr,
                      I2cAddrMode,
                      &WriteLength,
                      Data
                      );
  ASSERT_EFI_ERROR (Status);
}

//
// Routines exported by this source module.
//

/**
  Galileo FabE assert PCI express PERST# signal.

**/
VOID
EFIAPI
GalileoFabEPERSTAssert (
  VOID
  )
{
  EFI_STATUS                            Status;
  EFI_I2C_HC_PROTOCOL                  *I2cBus;

  Status = gBS->LocateProtocol (&gEfiI2CHcProtocolGuid, NULL, (VOID **) &I2cBus);
  ASSERT_EFI_ERROR (Status);

  //
  // Inverter after pin on GalileoGen2 so assert signal with high gpio value.
  //
  Pcal9555GpioSetLevel (I2cBus, PCAL9555_GALILEO_GEN2_7BIT_SLAVE_ADDR, PCAL9555_GALILEO_GEN2_PERST_GPIO, TRUE);
}

/**
  Galileo FabE de assert PCI express PERST# signal.

**/
VOID
EFIAPI
GalileoFabEPERSTDeAssert (
  VOID
  )
{
  EFI_STATUS                            Status;
  EFI_I2C_HC_PROTOCOL                  *I2cBus;

  Status = gBS->LocateProtocol (&gEfiI2CHcProtocolGuid, NULL, (VOID **) &I2cBus);
  ASSERT_EFI_ERROR (Status);

  //
  // Inverter after pin on GalileoFabE so de-assert signal with low gpio value.
  //
  Pcal9555GpioSetLevel (I2cBus, PCAL9555_GALILEO_GEN2_7BIT_SLAVE_ADDR, PCAL9555_GALILEO_GEN2_PERST_GPIO, FALSE);
}
