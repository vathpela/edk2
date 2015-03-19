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
#include <Guid/EventGroup.h>
#include <Guid/FakeCapsuleBase.h>

/**
  Iterate variables with the VendorGuid gFakeCapsuleHeaderGuid.  If
  CapsuleGuid is not NULL, only return those whose payload guid matches
  CapsuleGuid.

  @param  CapsuleGuid         Capsule payload guid to filter on.
  @param  VendorGuid          (used as iterator state)
  @param  VendorName          Name of variabel found (used as iterator state)
  @param  VendorData          Pool-Allocated space for vendor data.
  @param  VendorDataSize      Size that was allocated for *VendorData
  @param  VariableAttributes  Attributes of returned variable.

  @return EFI_INVALID_PARAMETER Invalid input
  @return EFI_OUT_OF_RESOURCES  Couldn't allocate space for name or data.
  @return EFI_NOT_FOUND         End of iteration.
**/
EFI_STATUS
EFIAPI
FindEsrtVariableByCapsuleGuid (
  IN EFI_GUID *CapsuleGuid OPTIONAL,
  IN OUT EFI_GUID *VendorGuid,
  IN OUT CHAR16 **VariableName,
  IN OUT VOID **VariableData,
  IN OUT UINTN *VariableDataSize,
  OUT UINT32 *VariableAttributes
  )
{
  RETURN_STATUS             Status;
  EFI_SYSTEM_RESOURCE_ENTRY *VarPayload;
  UINTN                     VariableNameBufferSize;
  UINTN                     VariableNameSize;
  UINTN                     VariableDataBufferSize;

  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));
  if (!VariableName) {
    return EFI_INVALID_PARAMETER;
  }

  if (!*VariableName) {
    VariableNameBufferSize = sizeof (CHAR16);
    *VariableName = AllocateZeroPool (VariableNameBufferSize);
    if (!*VariableName) {
      return EFI_OUT_OF_RESOURCES;
    }
  } else {
    VariableNameBufferSize = StrSize (*VariableName);
  }
  VariableNameSize = VariableNameBufferSize;

  VariableDataBufferSize = *VariableDataSize;

  while (1) {
    //
    // Get the next variable name and guid
    //
    Status = gRT->GetNextVariableName (&VariableNameSize,
                                       *VariableName,
                                       VendorGuid
                                       );
    if (Status == EFI_BUFFER_TOO_SMALL) {
      *VariableName = ReallocatePool (VariableNameBufferSize, VariableNameSize,
                                      *VariableName);
      VariableNameBufferSize = VariableNameSize;

      //
      // Try to get the next variable name again with the larger buffer.
      //
      Status = gRT->GetNextVariableName (&VariableNameSize,
                                         *VariableName,
                                         VendorGuid
                                        );
    }

    if (EFI_ERROR (Status)) {
      if (Status != EFI_NOT_FOUND) {
        DEBUG ((EFI_D_ERROR, "%a:%d: Error: %r (size is %d)\n",
                __FUNCTION__, __LINE__, Status, VariableNameSize));
      }
      break;
    }

    Status = EFI_NOT_FOUND;
    if (!CompareGuid(VendorGuid, &gFakeCapsuleHeaderGuid)) {
      continue;
    }

    //
    // Get the variable data and attributes
    //
    *VariableDataSize = VariableDataBufferSize;
    Status = gRT->GetVariable (*VariableName, VendorGuid, VariableAttributes,
                               VariableDataSize, *VariableData);
    if (Status == EFI_BUFFER_TOO_SMALL) {
      if (*VariableDataSize != sizeof (EFI_SYSTEM_RESOURCE_ENTRY)) {
        DEBUG ((EFI_D_VERBOSE, "%a:%d: %s wrong size (%d vs %d)\n",
		__FUNCTION__, __LINE__, VariableName, *VariableDataSize,
		sizeof (EFI_SYSTEM_RESOURCE_ENTRY)));
        continue;
      }

      *VariableData = ReallocatePool (VariableDataBufferSize, *VariableDataSize,
                                      *VariableData);
      VariableDataBufferSize = *VariableDataSize;

      //
      // Try to read the variable again with the larger buffer.
      //
      Status = gRT->GetVariable (*VariableName, VendorGuid,
                                 VariableAttributes, VariableDataSize,
                                 *VariableData);
    }

    if (EFI_ERROR (Status)) {
      break;
    }

    Status = EFI_NOT_FOUND;
    if (*VariableDataSize != sizeof (EFI_SYSTEM_RESOURCE_ENTRY)) {
      continue;
    }

    VarPayload = *VariableData;
    if (CapsuleGuid && !CompareGuid(CapsuleGuid, &VarPayload->FwClass)) {
      continue;
    }

    Status = EFI_SUCCESS;
    break;
  }

  if (EFI_ERROR (Status)) {
    if (Status != EFI_NOT_FOUND) {
      DEBUG ((EFI_D_ERROR, "%a:%d: Error: %r\n", __FUNCTION__, __LINE__,
              Status));
    }
    if (*VariableName != NULL) {
      FreePool (*VariableName);
      *VariableName = NULL;
    }

    if (*VariableData != NULL) {
      FreePool (*VariableData);
      *VariableData = NULL;
      *VariableDataSize = 0;
    }
  }
  return Status;
}

/**
  Find an unused Variable#### name within a given guid.

  @param  Name        Variable name that was found
  @param  Prefix      Prefix for Variable#### to search for.
  @param  VendorGuid  VendorGuid in which to search.
  @param  Limit       Maximum number to search for - cannot exceed 0xFFFF.

  @return EFI_NOT_FOUND         No variable could be found.
  @return EFI_OUT_OF_RESOURCES  No free memory to  allocate return value in.
**/
EFI_STATUS
EFIAPI
FindFreeNumberName (
  IN CHAR16 *Prefix,
  IN EFI_GUID *VendorGuid,
  IN UINT16 Limit,
  IN OUT UINT16 *Number
  )
{
  RETURN_STATUS Status;
  UINT32        i;
  CHAR16        *NameBuf;
  UINTN         BufferSize;
  UINT32        Attributes;
  UINTN         VariableDataSize = 0;

  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));

  if (!Number || !Prefix || !VendorGuid) {
    return EFI_INVALID_PARAMETER;
  }

  BufferSize = (StrLen(Prefix) + StrSize(L"0000")) * sizeof (CHAR16);
  NameBuf = AllocateZeroPool(BufferSize);
  if (!NameBuf) {
    return EFI_OUT_OF_RESOURCES;
  }

  for (i = *Number; i <= Limit; i++) {
    UnicodeSPrint (NameBuf, BufferSize, L"%s%04X", Prefix, i);

    Status = gRT->GetVariable (NameBuf,
                               VendorGuid,
                               &Attributes,
                               &VariableDataSize,
                               NULL
                               );
    if (Status == EFI_NOT_FOUND) {
      *Number = i;
      FreePool (NameBuf);
      return EFI_SUCCESS;
    }
  }
  if (*Number != 0 && i == Limit) {
    for (i = 0; i < *Number; i++) {
      UnicodeSPrint (NameBuf, BufferSize, L"%s%04X", Prefix, i);

      Status = gRT->GetVariable (NameBuf,
                                 VendorGuid,
                                 &Attributes,
                                 &VariableDataSize,
                                 NULL
                                 );
      if (Status == EFI_NOT_FOUND) {
        *Number = i;
        FreePool (NameBuf);
        return EFI_SUCCESS;
      }
    }
  }

  FreePool (NameBuf);

  return EFI_NOT_FOUND;
}

EFI_STATUS
EFIAPI
FakeCapsuleCountEntries(
  UINTN *NEntries
  )
{
  RETURN_STATUS             Status;
  EFI_GUID                  VendorGuid = { 0, };
  CHAR16                    *VariableName = NULL;
  VOID                      *VariableData = NULL;
  UINTN                     VariableDataSize;
  UINT32                    VariableAttributes = 0;

  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));
  if (!NEntries) {
    return EFI_INVALID_PARAMETER;
  }
  *NEntries = 0;

  VariableDataSize = sizeof (EFI_SYSTEM_RESOURCE_ENTRY);
  VariableData = AllocateZeroPool (VariableDataSize);
  if (!VariableData) {
    DEBUG ((EFI_D_ERROR, "%a:%d: AllocateZeroPool(%d) failed\n", __FUNCTION__,
            __LINE__, VariableDataSize));
    return EFI_OUT_OF_RESOURCES;
  }

  while (1) {
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
      DEBUG ((EFI_D_ERROR,
              "%a: FindEsrtVariableByCapsuleGuid failed: %r\n",
              __FUNCTION__,
              Status
             ));
      break;
    }
    (*NEntries)++;
  }

  if (VariableName) {
    FreePool (VariableName);
  }
  if (VariableData) {
    FreePool (VariableData);
  }

  return Status;
}
