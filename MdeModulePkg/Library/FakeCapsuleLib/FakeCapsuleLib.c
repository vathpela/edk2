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
#include <Library/FakeCapsuleBase.h>
#include <Library/Esrt.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Guid/EventGroup.h>
#include <Guid/FakeCapsuleBase.h>


/**
  The firmware checks whether the capsule image is supported by the CapsuleGuid
  in CapsuleHeader or other specific information in capsule image.

  @param  CapsuleHeader    Point to the UEFI capsule image to be checked.

  @retval EFI_UNSUPPORTED  Input capsule is not supported by the firmware.
**/
EFI_STATUS
EFIAPI
SupportCapsuleImage (
  IN EFI_CAPSULE_HEADER *CapsuleHeader
  )
{
  RETURN_STATUS             Status;
  EFI_SYSTEM_RESOURCE_ENTRY *CapPayload;
  EFI_GUID                  VendorGuid = { 0, };
  CHAR16                    *VariableName = NULL;
  VOID                      *VariableData = NULL;
  UINTN                     VariableDataSize = 0;
  UINT32                    VariableAttributes = 0;

  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));
  if (CapsuleHeader->CapsuleImageSize - CapsuleHeader->HeaderSize
      != sizeof (EFI_SYSTEM_RESOURCE_ENTRY)) {
    DEBUG ((EFI_D_ERROR,
            "%a:%d: CapsuleImageSize: %d hdr: %d sizeof (EFI_SYSTEM_RESOURCE_ENTRY): %d\n",
            __FUNCTION__, __LINE__,
            CapsuleHeader->CapsuleImageSize,
	    CapsuleHeader->HeaderSize,
	    sizeof (EFI_SYSTEM_RESOURCE_ENTRY)));
    return EFI_UNSUPPORTED;
  }

  CapPayload = (EFI_SYSTEM_RESOURCE_ENTRY *)
    ((UINT8 *)CapsuleHeader + CapsuleHeader->HeaderSize);

  Status = FindEsrtVariableByCapsuleGuid (&CapPayload->FwClass,
                                          &VendorGuid,
                                          &VariableName,
                                          &VariableData,
                                          &VariableDataSize,
                                          &VariableAttributes
                                         );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d: FindEsrtVariableByCapsuleGuid: %r\n",
            __FUNCTION__, __LINE__, Status));
    return EFI_UNSUPPORTED;
  }

  FreePool (VariableName);
  FreePool (VariableData);

  if (CapPayload->FwType > ESRT_FW_TYPE_UEFIDRIVER) {
    DEBUG ((EFI_D_ERROR, "%a:%d: CapPayload->FwType: %d\n",
            __FUNCTION__, __LINE__, CapPayload->FwType));
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

/**
  Convert a single character to number.
  It assumes the input Char is in the scope of L'0' ~ L'9' and L'A' ~ L'F'

  @param Char    The input char which need to change to a hex number.

 **/
STATIC
EFI_STATUS
EFIAPI
CharToUint (
  IN CHAR16 Char,
  OUT UINTN *Out
  )
{
  if (!Out) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Char >= L'0') && (Char <= L'9')) {
    *Out = (UINTN) (Char - L'0');
    return EFI_SUCCESS;
  }

  if ((Char >= L'A') && (Char <= L'F')) {
    *Out = (UINTN) (Char - L'A' + 0xA);
    return EFI_SUCCESS;
  }

  return EFI_INVALID_PARAMETER;
}

STATIC
EFI_STATUS
EFIAPI
GetCapsuleNumber(UINT16 *CapsuleNum, UINT16 Limit)
{
  RETURN_STATUS Status;
  CHAR16        VariableBuffer[] = L"Capsule0000";
  UINT32        Attributes;
  UINTN         VariableDataSize;
  UINTN         VariableNumber = 0;
  UINTN         i;

  if (!CapsuleNum) {
    return EFI_INVALID_PARAMETER;
  }

  VariableDataSize = sizeof (VariableBuffer);

  Status = gRT->GetVariable (L"CapsuleNum",
                             &gEfiCapsuleReportGuid,
                             &Attributes,
                             &VariableDataSize,
                             &VariableBuffer);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_BUFFER_TOO_SMALL || Status == EFI_NOT_FOUND) {
      *CapsuleNum = 0;
      return EFI_SUCCESS;
    }
    return Status;
  }

  if (VariableDataSize != StrLen(L"Capsule0000")) {
    *CapsuleNum = 0;
    return EFI_SUCCESS;
  }

  for (i = StrLen(L"Capsule"); i < StrLen(L"Capsule0000"); i++) {
    UINTN Num = 0;
    Status = CharToUint (*(VariableBuffer+i), &Num);
    if (EFI_ERROR (Status)) {
      *CapsuleNum = 0;
      return EFI_SUCCESS;
    }
    VariableNumber <<= 4;
    VariableNumber |= Num & 0x0f;
  }

  VariableNumber += 1;
  if (VariableNumber > Limit) {
    VariableNumber = 0;
  }

  *CapsuleNum = VariableNumber;
  return EFI_SUCCESS;
}

/**
  Find a Capsule Return Variable name we can use.

  @param  Name    Points to a pointer to the new name.
**/
STATIC
EFI_STATUS
EFIAPI
FindCapsuleReturnVarName (
  IN OUT CHAR16 **Name
  )
{
  RETURN_STATUS Status;
  UINTN         NEntries = 0;
  UINT16        VariableNumber = 0;

  Status = FakeCapsuleCountEntries (&NEntries);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d: FakeCapsuleCountEntries(): %r\n",
            __FUNCTION__, __LINE__, Status));
    return Status;
  }

  Status = GetCapsuleNumber (&VariableNumber, NEntries * 3);
  if (EFI_ERROR (Status) && Status != EFI_NOT_FOUND) {
    return Status;
  }
  *Name = AllocatePool(StrSize(L"Capsule0000"));
  if (!*Name) {
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeSPrint (*Name, StrSize(L"Capsule0000"), L"Capsule%04X",
                 VariableNumber);
  return EFI_SUCCESS;
}

/**
  Propogate LastAttemptStatus back to the variable data we're writing out and
  the Capsule#### variable, and set CapsuleLast.

  @param  CapsuleHeader     Points to the UEFI capsule image being processed.
  @param  CapPayload        Points to the EFI_SYSTEM_RESOURCE_ENTRY embedded
                            in the capsule being processed.
  @param  VarPayload        Points to the EFI_SYSTEM_RESOURCE_ENTRY loaded
                            from the already existing variable, if one exists.
  @param  LastAttemptStatus The status we're setting.
**/
STATIC
VOID
EFIAPI
ReportStatus (
  IN EFI_CAPSULE_HEADER *CapsuleHeader,
  IN EFI_SYSTEM_RESOURCE_ENTRY *CapPayload,
  IN EFI_SYSTEM_RESOURCE_ENTRY *VarPayload,
  IN CHAR16 *Name,
  IN UINT32 LastAttemptStatus
  )
{
  RETURN_STATUS                       Status;
  EFI_CAPSULE_RESULT_VARIABLE_HEADER  Result;

  DEBUG ((EFI_D_VERBOSE, "%a\n", __FUNCTION__));

  if (VarPayload) {
    VarPayload->LastAttemptStatus = LastAttemptStatus;
  }

  Result.VariableTotalSize = sizeof (Result);
  Result.Reserved = 0;
  CopyMem (&Result.CapsuleGuid, &CapPayload->FwClass, sizeof (EFI_GUID));
  gRT->GetTime (&Result.CapsuleProcessed, NULL);
  Result.CapsuleStatus = LastAttemptStatus;

  Status = gRT->SetVariable (Name,
                             &gEfiCapsuleReportGuid,
                             EFI_VARIABLE_NON_VOLATILE
                             | EFI_VARIABLE_BOOTSERVICE_ACCESS
                             | EFI_VARIABLE_RUNTIME_ACCESS,
                             sizeof (Result),
                             &Result
                             );
  if (!EFI_ERROR (Status)) {
    gRT->SetVariable (L"CapsuleLast",
                      &gEfiCapsuleReportGuid,
                      EFI_VARIABLE_NON_VOLATILE
                      | EFI_VARIABLE_BOOTSERVICE_ACCESS
                      | EFI_VARIABLE_RUNTIME_ACCESS,
                      StrLen (Name) * sizeof (Name[0]),
                      Name
                      );
  }
}

/**
  The firmware specific implementation processes the capsule image
  if it recognized the format of this capsule image.

  @param  CapsuleHeader    Point to the UEFI capsule image to be processed.

  @retval EFI_UNSUPPORTED  Capsule image is not supported by the firmware.
**/
EFI_STATUS
EFIAPI
ProcessCapsuleImage (
  IN EFI_CAPSULE_HEADER *CapsuleHeader
  )
{
  RETURN_STATUS             Status;
  EFI_SYSTEM_RESOURCE_ENTRY *CapPayload;
  EFI_GUID                  VendorGuid = { 0, };
  CHAR16                    *VariableName = NULL;
  VOID                      *VariableData = NULL;
  UINTN                     VariableDataSize = 0;
  UINT32                    VariableAttributes = 0;
  EFI_SYSTEM_RESOURCE_ENTRY *VarPayload;
  ESRT_OPERATION_PROTOCOL   *EsrtOps = NULL;
  CHAR16                    *RetVarName = NULL;

  DEBUG ((EFI_D_INFO, "%a\n", __FUNCTION__));
  CapPayload = (EFI_SYSTEM_RESOURCE_ENTRY *)
    ((UINT8 *)CapsuleHeader + CapsuleHeader->HeaderSize);

  Status = FindCapsuleReturnVarName (&RetVarName);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d FindCapsuleReturnVarName: %r\n",
            __FUNCTION__, __LINE__, Status));
    return Status;
  }

  Status = FindEsrtVariableByCapsuleGuid (&CapPayload->FwClass,
                                          &VendorGuid,
                                          &VariableName,
                                          &VariableData,
                                          &VariableDataSize,
                                          &VariableAttributes
                                          );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d FindEsrtVariableByCapsuleGuid: %r\n",
            __FUNCTION__, __LINE__, Status));
    ReportStatus (CapsuleHeader,
                  CapPayload,
                  NULL,
                  RetVarName,
                  Status == EFI_OUT_OF_RESOURCES
                  ? LAST_ATTEMPT_STATUS_ERROR_INSUFFICIENT_RESOURCES
                  : LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL
                  );
    return Status;
  }

  VarPayload = (EFI_SYSTEM_RESOURCE_ENTRY *)VariableData;
  VarPayload->LastAttemptVersion = CapPayload->FwVersion;

  Status = gBS->LocateProtocol (&gEfiEsrtOperationProtocolGuid, NULL,
                                (VOID **)&EsrtOps);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR,
            "%a:%d LocateProtocol(gEfiEsrtOperationProtocolGuid): %r\n",
            __FUNCTION__, __LINE__, Status));
    ReportStatus (CapsuleHeader,
                  CapPayload,
                  NULL,
                  RetVarName,
                  LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL
                  );
    FreePool (VariableName);
    FreePool (VariableData);
    FreePool (RetVarName);

    return Status;
  }

  if ((CapPayload->CapsuleFlags & 0xffffu) !=
      (VarPayload->CapsuleFlags & 0xffffu)) {
    DEBUG ((EFI_D_ERROR,
            "%a:%d CapsuleFlags don't match (%04x:%04x)\n",
            __FUNCTION__, __LINE__,
            CapPayload->CapsuleFlags, VarPayload->CapsuleFlags));
    ReportStatus (CapsuleHeader,
                  CapPayload,
                  VarPayload,
                  RetVarName,
                  LAST_ATTEMPT_STATUS_ERROR_INVALID_FORMAT
                  );

    FreePool (VariableName);
    FreePool (VariableData);
    FreePool (RetVarName);

    return EFI_INVALID_PARAMETER;
  }

  if (CapPayload->FwVersion < VarPayload->LowestSupportedFwVersion) {
    DEBUG ((EFI_D_ERROR,
            "%a:%d Version mismatch %032x:%032x\n",
            __FUNCTION__, __LINE__,
            CapPayload->FwVersion,
            VarPayload->LowestSupportedFwVersion));
    ReportStatus (CapsuleHeader,
                  CapPayload,
                  VarPayload,
                  RetVarName,
                  LAST_ATTEMPT_STATUS_ERROR_INCORRECT_VERSION
                  );

    FreePool (VariableName);
    FreePool (VariableData);
    FreePool (RetVarName);

    return EFI_INVALID_PARAMETER;
  } else {
    VarPayload->LastAttemptStatus = LAST_ATTEMPT_STATUS_SUCCESS;
    VarPayload->FwVersion = CapPayload->FwVersion;

    DEBUG ((EFI_D_ERROR, "%a:%d success\n", __FUNCTION__, __LINE__));
    ReportStatus (CapsuleHeader,
                  CapPayload,
                  VarPayload,
                  RetVarName,
                  LAST_ATTEMPT_STATUS_SUCCESS
                  );
  }

  //
  // This is what actually determines if the firmware update has really
  // happened...
  //
  Status = gRT->SetVariable (VariableName,
                             &gFakeCapsuleHeaderGuid,
                             VariableAttributes,
                             sizeof (EFI_SYSTEM_RESOURCE_ENTRY),
                             VarPayload
                             );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "%a:%d: SetVariable() == %r\n",
            __FUNCTION__, __LINE__, Status));
    //
    // If that actually didn't work, we need to re-write out Capsule####
    // variable.  Which means we can only actually trust it if it agrees with
    // ESRT.
    ReportStatus (CapsuleHeader,
                  CapPayload,
                  VarPayload,
                  RetVarName,
                  Status == EFI_OUT_OF_RESOURCES
                  ? LAST_ATTEMPT_STATUS_ERROR_INSUFFICIENT_RESOURCES
                  : LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL
                  );
  } else {
    //
    // this just updates the live ESRT; in most cases we'll reboot, but we
    // should try to accommodate the times we don't.
    //
    EFI_STATUS Status2;
    DEBUG ((EFI_D_ERROR,
	    "%a:%d: calling EsrtOps->EsrtUpdateTableEntryByGuid(%g,0x%08x)\n",
	    __FUNCTION__, __LINE__, CapsuleHeader->CapsuleGuid,
	    VarPayload));
    Status2 = EsrtOps->EsrtUpdateTableEntryByGuid(CapsuleHeader->CapsuleGuid,
                                        VarPayload);
    if (EFI_ERROR (Status2)) {
      DEBUG ((EFI_D_ERROR, "%a:%d: EsrtUpdateTableEntryByGuid() == %r\n",
              __FUNCTION__, __LINE__, Status2));
    }
  }

  FreePool (VariableName);
  FreePool (VariableData);
  FreePool (RetVarName);

  return Status;
}
