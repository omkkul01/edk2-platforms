/** @file
  Driver to handle and support all platform errors.

  Installs the SDEI and HEST ACPI tables for firmware first error handling.

  Copyright (c) 2020 - 2021, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - ACPI 6.3, Table 18-382, Hardware Error Source Table
    - SDEI Platform Design Document, revision b, 10 Appendix C, ACPI table
      definitions for SDEI
**/

#include <IndustryStandard/Acpi.h>

#include <Library/AcpiLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/AcpiTable.h>
#include <Protocol/HestTable.h>


/**
  Build and install the SDEI ACPI table.

  For platforms that allow firmware-first platform error handling, SDEI is used
  as the notification mechanism for those errors.

  @retval EFI_SUCCESS  SDEI table installed successfully.
  @retval Other        For any error during installation.
**/
STATIC
EFI_STATUS
InstallSdeiTable (VOID)
{
  EFI_ACPI_TABLE_PROTOCOL     *mAcpiTableProtocol = NULL;
  EFI_ACPI_DESCRIPTION_HEADER Header;
  EFI_STATUS                  Status;
  UINTN                       AcpiTableHandle;

  Header =
    (EFI_ACPI_DESCRIPTION_HEADER) {
     EFI_ACPI_6_3_SOFTWARE_DELEGATED_EXCEPTIONS_INTERFACE_TABLE_SIGNATURE,
     sizeof (EFI_ACPI_DESCRIPTION_HEADER), // Length
     0x01,                                 // Revision
     0x00,                                 // Checksum
     {'A', 'R', 'M', 'L', 'T', 'D'},       // OemId
     0x4152464e49464552,                   // OemTableId:"REFINFRA"
     0x20201027,                           // OemRevision
     0x204d5241,                           // CreatorId:"ARM "
     0x00000001,                           // CreatorRevision
    };

  Header.Checksum = CalculateCheckSum8 ((UINT8 *)&Header, Header.Length);
  Status = gBS->LocateProtocol (
                  &gEfiAcpiTableProtocolGuid,
                  NULL,
                  (VOID **)&mAcpiTableProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate ACPI table protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
    return Status;
  }

  Status = mAcpiTableProtocol->InstallAcpiTable (
                                 mAcpiTableProtocol,
                                 &Header,
                                 Header.Length,
                                 &AcpiTableHandle
                                 );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install SDEI ACPI table, status: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

/**
  Install the HEST ACPI table.

  HEST ACPI table is used to list the platform errors for which the error
  handling has been supported. Use the HEST table generation protocol to
  install the HEST table.

  @retval EFI_SUCCESS  HEST table installed successfully.
  @retval Other        For any error during installation.
**/
STATIC
EFI_STATUS
InstallHestTable (VOID)
{
  HEST_TABLE_PROTOCOL *HestProtocol;
  EFI_STATUS          Status;

  Status = gBS->LocateProtocol (
                  &gHestTableProtocolGuid,
                  NULL,
                  (VOID **)&HestProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to locate HEST DXE Protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
      return Status;
  }

  Status = HestProtocol->InstallHestTable ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to install HEST table, status: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}

/**
  Entry point for the DXE driver.

  This function installs the HEST ACPI table, using the HEST table generation
  protocol. Also creates and installs the SDEI ACPI table required for firmware
  first error handling.

  @param[in]  ImageHandle  Handle to the EFI image.
  @param[in]  SystemTable  A pointer to the EFI System Table.

  @retval  EFI_SUCCESS  On successful installation of ACPI tables
  @retval  Other        On Failure
**/
EFI_STATUS
EFIAPI
PlatformErrorHandlerEntryPoint (
  IN EFI_HANDLE       ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  // Build and install SDEI table.
  Status = InstallSdeiTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Install the created HEST table.
  Status = InstallHestTable ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
