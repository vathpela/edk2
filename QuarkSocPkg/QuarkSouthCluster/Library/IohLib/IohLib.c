/** @file
  Lib function for Pei Quark South Cluster.

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

#define PCI_IOSF2AHB_0_MAX_FUNCS     7
#define PCI_IOSF2AHB_1_MAX_FUNCS     3

/**
 Program SVID/SID the same as VID/DID* 
**/
EFI_STATUS
EFIAPI
InitializeIohSsvidSsid (
   IN UINT8   Bus,
   IN UINT8   Device,
   IN UINT8   Func
   )
{
    UINTN       j = 0;

	for (j = 0; j <= PCI_IOSF2AHB_0_MAX_FUNCS; j++)
	{
		if (((Device == PCI_IOSF2AHB_1_DEV_NUM) && (j >= PCI_IOSF2AHB_1_MAX_FUNCS)))
		{
			continue;
		}

		IohMmPci32(0, Bus, Device, j, PCI_REG_SVID0) = IohMmPci32(0, Bus, Device, j, PCI_REG_VID);
	}

    return EFI_SUCCESS;
}

/* Enable memory, io, and bus master for USB controller */
VOID
EFIAPI
EnableUsbMemIoBusMaster (
		IN UINT8   UsbBusNumber
  )
{
  UINT16 CmdReg;

	CmdReg = PciRead16 (PCI_LIB_ADDRESS (UsbBusNumber, IOH_USB_OHCI_DEVICE_NUMBER, IOH_OHCI_FUNCTION_NUMBER, PCI_REG_PCICMD));
	CmdReg = (UINT16) (CmdReg | 0x7);
	PciWrite16 (PCI_LIB_ADDRESS (UsbBusNumber, IOH_USB_OHCI_DEVICE_NUMBER, IOH_OHCI_FUNCTION_NUMBER, PCI_REG_PCICMD), CmdReg);

	CmdReg = PciRead16 (PCI_LIB_ADDRESS (UsbBusNumber, IOH_USB_EHCI_DEVICE_NUMBER, IOH_EHCI_FUNCTION_NUMBER, PCI_REG_PCICMD));
	CmdReg = (UINT16) (CmdReg | 0x7);
	PciWrite16 (PCI_LIB_ADDRESS (UsbBusNumber, IOH_USB_EHCI_DEVICE_NUMBER, IOH_EHCI_FUNCTION_NUMBER, PCI_REG_PCICMD), CmdReg);
}

