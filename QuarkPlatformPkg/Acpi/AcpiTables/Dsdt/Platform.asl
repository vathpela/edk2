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

    Platform.asl

  Abstract:

    Contains root level name space objects for the platform

--*/


//
// OS TYPE DEFINITION
//
#define WINDOWS_XP          0x01
#define WINDOWS_XP_SP1      0x02
#define WINDOWS_XP_SP2      0x04
#define WINDOWS_2003        0x08
#define WINDOWS_Vista       0x10
#define WINDOWS_WIN7        0x11
#define LINUX               0xF0

DefinitionBlock (
  "Platform.aml",
  "DSDT",
  1,
  "INTEL ",
  "QuarkNcSocId",
  3)
{
    //
    // Global Variables
    //
    Name(\GPIC, 0x0)

    //
    // Port 80
    // 
    OperationRegion (DBG0, SystemIO, 0x80, 1)
    Field (DBG0, ByteAcc, NoLock, Preserve)
    { IO80,8 }

    //
    // Access CMOS range
    //
    OperationRegion (ACMS, SystemIO, 0x72, 2)
    Field (ACMS, ByteAcc, NoLock, Preserve)
    { INDX, 8, DATA, 8 }

    //
    // Global NVS Memory Block
    //
    OperationRegion (MNVS, SystemMemory, 0xFFFF0000, 512)
    Field (MNVS, ByteAcc, NoLock, Preserve)
    {
      OSTP, 32,
      CFGD, 32,
      HPEA, 32,  // HPET Enabled ?

      P1BB, 32,  // Pm1blkIoBaseAddress;
      PBAB, 32,  // PmbaIoBaseAddress;  
      GP0B, 32,  // Gpe0blkIoBaseAddress;
      GPAB, 32,  // GbaIoBaseAddress;

      SMBB, 32,  // SmbaIoBaseAddress;
      SPIB, 32,  // SpiDmaIoBaseAddress;
      WDTB, 32,  // WdtbaIoBaseAddress;

      HPTB, 32,  // HpetBaseAddress;
      HPTS, 32,  // HpetSize;
      PEXB, 32,  // PciExpressBaseAddress;
      PEXS, 32,  // PciExpressSize;

      RCBB, 32,  // RcbaMmioBaseAddress;
      RCBS, 32,  // RcbaMmioSize;
      APCB, 32,  // IoApicBaseAddress;
      APCS, 32,  // IoApicSize;

      TPMP, 32,  // TpmPresent ?
    }

    OperationRegion (GPEB, SystemIO, 0x1100, 0x40)  //GPE Block
    Field (GPEB, AnyAcc, NoLock, Preserve)
    {
      Offset(0x10),
      SMIE, 32,                 // SMI Enable
      SMIS, 32,                 // SMI Status
    }

    //
    //  Processor Objects
    //
    Scope(\_PR) {
        //
        // IO base will be updated at runtime with search key "PRIO"
        //
        Processor (CPU0, 0x01, 0x4F495250, 0x06) {}
    }

    //
    // System Sleep States
    //
    Name (\_S0,Package (){0,0,0,0})
    Name (\_S3,Package (){5,0,0,0})
    Name (\_S4,Package (){6,0,0,0})
    Name (\_S5,Package (){7,0,0,0})

    //
    //  General Purpose Event
    //
    Scope(\_GPE)
    {
        //
        // EGPE generated GPE
        //
        Method(_L0D, 0x0, NotSerialized)
        {
            //
            // Check EGPE for this wake event
            //
            Notify (\_SB.SLPB, 0x02)

        }

        //
        // GPIO generated GPE
        //
        Method(_L0E, 0x0, NotSerialized)
        {
            //
            // Check GPIO for this wake event
            //
            Notify (\_SB.PWRB, 0x02)

        }

        //
        // SCLT generated GPE
        //
        Method(_L0F, 0x0, NotSerialized)
        {
            //
            // Check SCLT for this wake event
            //
            Notify (\_SB.PCI0.SDIO, 0x02)
            Notify (\_SB.PCI0.URT0, 0x02)
            Notify (\_SB.PCI0.USBD, 0x02)
            Notify (\_SB.PCI0.EHCI, 0x02)
            Notify (\_SB.PCI0.OHCI, 0x02)
            Notify (\_SB.PCI0.URT1, 0x02)
            Notify (\_SB.PCI0.ENT0, 0x02)
            Notify (\_SB.PCI0.ENT1, 0x02)
            Notify (\_SB.PCI0.SPI0, 0x02)
            Notify (\_SB.PCI0.SPI1, 0x02)
            Notify (\_SB.PCI0.GIP0, 0x02)

        }

        //
        // Remote Management Unit generated GPE
        //
        Method(_L10, 0x0, NotSerialized)
        {
            //
            // Check Remote Management Unit for this wake event.
            //
        }

        //
        // PCIE generated GPE
        //
        Method(_L11, 0x0, NotSerialized)
        {
            //
            // Check PCIE for this wake event
            //
            Notify (\_SB.PCI0.PEX0, 0x02)
            Notify (\_SB.PCI0.PEX1, 0x02)
        }
    }

    //
    // define Sleeping button as mentioned in ACPI spec 2.0
    //
    Device (\_SB.SLPB)
    {
        Name (_HID, EISAID ("PNP0C0E"))
        Method (_PRW, 0, NotSerialized)
        {
            Return (Package (0x02) {0x0D,0x04})
        }
    }

    //
    // define Power Button
    //
     Device (\_SB.PWRB)
    {
        Name (_HID, EISAID ("PNP0C0C"))
        Method (_PRW, 0, NotSerialized)
        {
            Return (Package (0x02) {0x0E,0x04})
        }
    }
    //
    // System Wake up
    //
    Method(_WAK, 1, Serialized)
    {
       // Do nothing here 
       Return (0)
    }

    //
    // System sleep down
    //
    Method (_PTS, 1, NotSerialized)
    {
        // Get ready for S3 sleep
        if (Lequal(Arg0,3))
        {
                Store(0xffffffff,SMIS)     // clear SMI status
                Store(SMIE, Local0)        // SMI Enable
                Or(Local0,0x4,SMIE)        // Generate SMI on sleep
        }
    }

    //
    // Determing PIC mode
    //
    Method(\_PIC, 1, NotSerialized)
    {
        Store(Arg0,\GPIC)
    }

    //
    //  System Bus
    //
    Scope(\_SB)
    {
        Device(PCI0)
        {
            Name(_HID,EISAID ("PNP0A08"))          // PCI Express Root Bridge
            Name(_CID,EISAID ("PNP0A03"))          // Compatible PCI Root Bridge

            Name(_ADR,0x00000000)                  // Device (HI WORD)=0, Func (LO WORD)=0
            Method (_INI)
            {
                Store(LINUX, OSTP)                 // Set the default os is Linux
                If (CondRefOf (_OSI, local0))
                {
                    //
                    //_OSI is supported, so it is WinXp or Win2003Server
                    //
                    If (\_OSI("Windows 2001"))
                    {
                        Store (WINDOWS_XP, OSTP)
                    }
                    If (\_OSI("Windows 2001 SP1"))
                    {
                        Store (WINDOWS_XP_SP1, OSTP)
                    }
                    If (\_OSI("Windows 2001 SP2"))
                    {
                        Store (WINDOWS_XP_SP2, OSTP)
                    }
                    If (\_OSI("Windows 2001.1"))
                    {
                        Store (WINDOWS_2003, OSTP)
                    }
                    If (\_OSI("Windows 2006"))
                    {
                        Store (WINDOWS_Vista, OSTP)
                    }
                    If (\_OSI("Windows 2009"))
                    {
                        Store (WINDOWS_WIN7, OSTP)
                    }
                    If (\_OSI("Linux"))
                    {
                      Store (LINUX, OSTP)
                    }
                }
            }

            Include ("PciHostBridge.asi")    // PCI0 Host bridge
            Include ("QNC.asi")               // QNC miscellaneous
            Include ("PcieExpansionPrt.asi") // PCIe expansion bridges/devices
            Include ("QuarkSouthCluster.asi") // Quark South Cluster devices
            Include ("QNCLpc.asi")            // LPC bridge device
            Include ("QNCApic.asi")           // QNC I/O Apic device
        }
        Include ("Tpm.asi")                   // TPM device
    }
}
