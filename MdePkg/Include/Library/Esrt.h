/*++

  Copyright (c) 2004  - 2014, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials are licensed and made
  available under the terms and conditions of the BSD License that
  accompanies this distribution.  The full text of the license may be
  found at http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
  IMPLIED.

Module Name:

 Esrt.h

Abstract:

--*/

#ifndef _DFU_ESRT_H_
#define _DFU_ESRT_H_

typedef struct {
  UINT32                    NumEntries;
  EFI_SYSTEM_RESOURCE_ENTRY FwEntries[256];
} EFI_SYSTEM_RESOURCE_ENTRY_LIST;

typedef
EFI_STATUS
(EFIAPI *ESRT_POPULATE_TABLE) (
);

typedef
EFI_STATUS
(EFIAPI *ESRT_UPDATE_TABLE_ENTRY_BY_GUID) (
  IN EFI_GUID                   FwEntryGuid,
  IN EFI_SYSTEM_RESOURCE_ENTRY  *FwEntry
);

typedef
EFI_STATUS
(EFIAPI *ESRT_GET_FW_ENTRY_BY_GUID) (
  IN EFI_GUID                   FwEntryGuid,
  OUT EFI_SYSTEM_RESOURCE_ENTRY *FwEntry
);

#pragma pack(1)
typedef struct _ESRT_OPERATION_PROTOCOL {
  ESRT_POPULATE_TABLE             EsrtPopulateTable;
  ESRT_UPDATE_TABLE_ENTRY_BY_GUID EsrtUpdateTableEntryByGuid;
  ESRT_GET_FW_ENTRY_BY_GUID       EsrtGetFwEntryByGuid;
} ESRT_OPERATION_PROTOCOL;

extern EFI_GUID gEfiEsrtOperationProtocolGuid;
extern EFI_GUID gEfiEsrtTableGuid;

#endif
