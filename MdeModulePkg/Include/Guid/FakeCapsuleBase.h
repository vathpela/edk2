/** @file
  GUID to be used to identify FakeCapsuleLib capsule payloads.

  Copyright (C) 2015, Red Hat, Inc.

  This program and the accompanying materials are licensed and made
  available under the terms and conditions of the BSD License that
  accompanies this distribution. The full text of the license may be found
  at http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR
  IMPLIED.

**/

#ifndef __FAKE_CAPSULE_BASE__
#define __FAKE_CAPSULE_BASE__

#define FAKE_CAPSULE_HEADER_GUID \
{0x32e678e9, 0x263f, 0x4eae, {0xa2, 0x39, 0x38, 0xa3, 0xca, 0xd6, 0xa1, 0xb5}}

#define EFI_CAPSULE_REPORT_GUID \
{0x39b68c46, 0xf7fb, 0x441b, {0xb6, 0xec, 0x16, 0xb0, 0xf6, 0x98, 0x21, 0xf3}}

extern EFI_GUID gFakeCapsuleHeaderGuid;
extern EFI_GUID gEfiCapsuleReportGuid;

#endif
