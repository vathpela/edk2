/** @file
  Fake Dxe Capsule Library instance that just keeps ESRT data in UEFI
  Variables.

  Copyright 2015 Red Hat, Inc.

  This program and the accompanying materials are licensed and made
  available under the terms and conditions of the BSD License which
  accompanies this distribution.  The full text of the license may be
  found at http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
  IMPLIED.

**/
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CapsuleLib.h>
#include <Library/DebugLib.h>
#include <Library/Esrt.h>
#include <Library/FakeCapsuleBase.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/FakeCapsuleBase.h>
#include <Guid/EventGroup.h>

STATIC EFI_SYSTEM_RESOURCE_TABLE *GlobalEsrt;
STATIC UINTN GlobalEsrtSize;

/**
  Find an ESRT entry by the FwClass GUID

  @param  FwEntryGuid   GUID to use as lookup key
  @param  FwEntry       Storage space to hole the entry in.

  @return EFI_NOT_FOUND No such entry could be located.
**/
STATIC
EFI_STATUS
EFIAPI
FakeEsrtGetFwEntryByGuid (
  IN EFI_GUID FwEntryGuid,
  OUT EFI_SYSTEM_RESOURCE_ENTRY *FwEntry
  )
{
  RETURN_STATUS Status;
  CHAR16        *VariableName = NULL;
  VOID          *VariableData = NULL;
  UINTN         VariableDataSize = 0;
  UINT32        VariableAttributes;
  EFI_GUID      VendorGuid = { 0, };

  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_ERROR, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_INFO, "%a\n", __FUNCTION__));

  Status = FindEsrtVariableByCapsuleGuid (&FwEntryGuid,
                                          &VendorGuid,
                                          &VariableName,
                                          &VariableData,
                                          &VariableDataSize,
                                          &VariableAttributes);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "%a: FindEsrtVariableByCapsuleGuid failed: %r\n",
            __FUNCTION__,
            Status
           ));
    return Status;
  }

  FreePool (VariableName);
  CopyMem (FwEntry, VariableData, VariableDataSize);
  FreePool (VariableData);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
FakeEsrtUpdateConfigTableEntry (
  EFI_SYSTEM_RESOURCE_ENTRY *FwEntry
  )
{
  EFI_SYSTEM_RESOURCE_ENTRY *Entry;
  UINTN                     i;

  DEBUG ((EFI_D_INFO, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_ERROR, "%a\n", __FUNCTION__));
  if (GlobalEsrt == NULL ||
      GlobalEsrtSize <= sizeof (EFI_SYSTEM_RESOURCE_TABLE)) {
    return EFI_NOT_FOUND;
  }

  Entry = (EFI_SYSTEM_RESOURCE_ENTRY *)(GlobalEsrt + 1);
  for (i = 0; i < GlobalEsrt->FwResourceCount; i++) {
    if (CompareGuid (&FwEntry->FwClass, &Entry[i].FwClass)) {
      CopyMem (Entry+i, FwEntry, sizeof (*FwEntry));
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}


//
// set new values in an ESRT entry
//
STATIC
EFI_STATUS
EFIAPI
FakeEsrtUpdateTableEntryByGuid (
  IN EFI_GUID FwEntryGuid,
  IN EFI_SYSTEM_RESOURCE_ENTRY *FwEntry
  )
{
  RETURN_STATUS Status;
  EFI_GUID      VendorGuid = { 0, };
  CHAR16        *VariableName = L"Esrt0000";
  VOID          *VariableData = NULL;
  UINTN         VariableDataSize = sizeof (EFI_SYSTEM_RESOURCE_ENTRY);
  UINT32        VariableAttributes;
  UINTN         NEntries = 0;
  UINT16        VariableNumber = 0;

  DEBUG ((EFI_D_INFO, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_ERROR, "%a\n", __FUNCTION__));
  Status = FakeCapsuleCountEntries(&NEntries);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d FakeCapsuleCountEntries(): %r\n",
            __FUNCTION__, __LINE__, Status));
    return Status;
  }

  VariableData = AllocateZeroPool (VariableDataSize);
  if (!VariableData) {
    DEBUG ((EFI_D_ERROR, "%a:%d AllocateZeroPool(): %r\n",
            __FUNCTION__, __LINE__, EFI_OUT_OF_RESOURCES));
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (VariableData, FwEntry, sizeof (EFI_SYSTEM_RESOURCE_ENTRY));
  Status = FindEsrtVariableByCapsuleGuid (&FwEntryGuid,
                                          &VendorGuid,
                                          &VariableName,
                                          &VariableData,
                                          &VariableDataSize,
                                          &VariableAttributes
                                          );
  FreePool (VariableData);
  VariableData = NULL;
  if (Status == EFI_NOT_FOUND) {
    Status = FindFreeNumberName (L"Esrt",
                                 &gFakeCapsuleHeaderGuid,
                                 NEntries * 3,
                                 &VariableNumber);
    if (EFI_ERROR (Status)) {
      if (Status == EFI_NOT_FOUND) {
        Status = EFI_OUT_OF_RESOURCES;
      }
      return Status;
    }
  }

  UnicodeSPrint (VariableName, StrSize(L"Esrt0000"), L"Esrt%04X",
                 VariableNumber);
  Status = gRT->SetVariable(VariableName,
                            &gFakeCapsuleHeaderGuid,
                            EFI_VARIABLE_NON_VOLATILE
                            | EFI_VARIABLE_BOOTSERVICE_ACCESS
                            | EFI_VARIABLE_RUNTIME_ACCESS,
                            sizeof (EFI_SYSTEM_RESOURCE_ENTRY),
                            FwEntry
                            );

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d SetVariable(%s): %r\n",
            __FUNCTION__, __LINE__, VariableName, Status));
  }
  FreePool (VariableName);

  Status = FakeEsrtUpdateConfigTableEntry (FwEntry);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d FakeEsrtUpdateConfigTableEntry(): %r\n",
            __FUNCTION__, __LINE__, Status));
  }
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
FakeEsrtPopulateTable (
  )
{
  RETURN_STATUS             Status;
  CHAR16                    *VariableName = NULL;
  VOID                      *VariableData = NULL;
  UINTN                     VariableDataSize;
  UINT32                    VariableAttributes = 0;
  EFI_GUID                  VendorGuid = { 0, };
  UINTN                     NEntries = 0;
  EFI_SYSTEM_RESOURCE_TABLE Esrt;
  UINTN                     TableSize;
  EFI_PHYSICAL_ADDRESS      TableAddr = 0;
  UINT8                     *TableBuf;
  UINTN                     i;
  CHAR16                    CapsuleMax[] = L"Capsule0000";
  UINTN                     NChars;
  VOID                      *Esre;

  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_INFO, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_ERROR, "%a\n", __FUNCTION__));
  Status = FakeCapsuleCountEntries(&NEntries);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  VariableDataSize = sizeof (EFI_SYSTEM_RESOURCE_ENTRY);
  VariableData = AllocateZeroPool (VariableDataSize);
  if (!VariableData) {
    DEBUG ((EFI_D_ERROR, "%a:%d AllocateZeroPool(%d) failed\n",
            __FUNCTION__, __LINE__, VariableDataSize));
    return EFI_OUT_OF_RESOURCES;
  }

  TableSize = ALIGN_VALUE(sizeof (Esrt)
                          + NEntries * sizeof (EFI_SYSTEM_RESOURCE_ENTRY),
                          4096
                          );

  Status = gBS->AllocatePages (AllocateAnyPages,
                               EfiRuntimeServicesData,
                               TableSize / 4096,
                               &TableAddr
                               );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d AllocatePages(%d) failed: %r\n",
            __FUNCTION__, __LINE__, TableSize, Status));
    return Status;
  }
  TableBuf = (UINT8 *)(UINTN)TableAddr;

  Esrt.FwResourceVersion = 1;
  Esrt.FwResourceMax = (TableSize - sizeof (Esrt))
                       / sizeof (EFI_SYSTEM_RESOURCE_ENTRY);
  Esrt.FwResourceCount = 0;

  Esre = TableBuf + sizeof (Esrt);

  for (i = 0; i < NEntries; i++) {
    Esre = TableBuf + sizeof (Esrt) + sizeof (EFI_SYSTEM_RESOURCE_ENTRY) * i;
    VariableData = NULL;
    VariableDataSize = 0;
    Status = FindEsrtVariableByCapsuleGuid (NULL,
                                            &VendorGuid,
                                            &VariableName,
                                            &VariableData,
                                            &VariableDataSize,
                                            &VariableAttributes
                                            );
    if (Status == EFI_NOT_FOUND) {
      Status = EFI_SUCCESS;
      break;
    }
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a:%d: FindEsrtVariableByCapsuleGuid failed: %r\n",
              __FUNCTION__, __LINE__, Status));
      break;
    }
    if (VariableData && VariableDataSize) {
      CopyMem (Esre, VariableData, VariableDataSize);
      FreePool (VariableData);
      VariableData = NULL;
    }
  }
  if (VariableName) {
    FreePool (VariableName);
    VariableName = NULL;
  }

  Esrt.FwResourceCount = i;
  CopyMem (TableBuf, &Esrt, sizeof (Esrt));

  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d: Couldn't get variable: %r\n",
            __FUNCTION__, __LINE__, Status));
    gBS->FreePages (TableAddr, TableSize / 4096);
    return Status;
  }

  Status = gBS->InstallConfigurationTable (&gEfiEsrtTableGuid, TableBuf);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d InstallConfigurationTable failed: %r\n",
            __FUNCTION__, __LINE__, Status));
    gBS->FreePages (TableAddr, TableSize / 4096);
    return Status;
  }

  GlobalEsrt = (EFI_SYSTEM_RESOURCE_TABLE *)TableBuf;
  GlobalEsrtSize = TableSize;

  NChars = UnicodeSPrint (CapsuleMax, sizeof (CapsuleMax), L"Capsule%04X",
                          NEntries * 3);
  if (NChars != 11) {
    DEBUG ((EFI_D_ERROR, "%a:%d UnicodeSPrint(Capsule####)=%d (should be 11)\n",
            __FUNCTION__,__LINE__, NChars));
    gBS->FreePages (TableAddr, TableSize / 4096);
    return EFI_ABORTED;
  }
  Status = gRT->SetVariable (L"CapsuleMax",
                             &gEfiCapsuleReportGuid,
                             EFI_VARIABLE_NON_VOLATILE
                             | EFI_VARIABLE_BOOTSERVICE_ACCESS
                             | EFI_VARIABLE_RUNTIME_ACCESS,
                             NChars * sizeof (CHAR16),
                             CapsuleMax
                             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d: SetVariable(CapsuleMax) failed: %r\n",
            __FUNCTION__, __LINE__, Status));
  }

  return Status;
}

STATIC CONST ESRT_OPERATION_PROTOCOL mFakeEsrtOperationProtocol = {
  FakeEsrtPopulateTable,
  FakeEsrtUpdateTableEntryByGuid,
  FakeEsrtGetFwEntryByGuid,
};

EFI_STATUS
EFIAPI
FakeEsrtDxeEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  RETURN_STATUS Status;

  DEBUG ((EFI_D_ERROR, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));
  DEBUG ((EFI_D_INFO, "%a\n", __FUNCTION__));

  /* Ideally this would be called by some /consumer/ of
   * gEfiEsrtOperationProtocolGuid, but there's not much of that existing
   * just yet in the actual tiano tree. */
  FakeEsrtPopulateTable();

  Status = gBS->InstallMultipleProtocolInterfaces (&ImageHandle,
                                                &gEfiEsrtOperationProtocolGuid,
                                                &mFakeEsrtOperationProtocol,
                                                NULL
                                                );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a: InstallMultipleProtocolInterfaces failed: %r\n",
            __FUNCTION__, Status));
  }
  return Status;
}
