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
#ifndef _FAKE_CAPSULE_H
#define _FAKE_CAPSULE_H

EFI_STATUS
EFIAPI
FindEsrtVariableByCapsuleGuid (
  IN EFI_GUID *CapsuleGuid OPTIONAL,
  IN OUT EFI_GUID *VendorGuid,
  IN OUT CHAR16 **VariableName,
  IN OUT VOID **VariableData,
  IN OUT UINTN *VariableDataSize,
  OUT UINT32 *VariableAttributes
  );

EFI_STATUS
EFIAPI
FindFreeNumberName (
  IN CHAR16 *Prefix,
  IN EFI_GUID *VendorGuid,
  IN UINT16 Limit,
  IN OUT UINT16 *Number
  );

EFI_STATUS
EFIAPI
FakeCapsuleCountEntries(
  UINTN *NEntries
  );

#endif /* _FAKE_CAPSULE_H */
