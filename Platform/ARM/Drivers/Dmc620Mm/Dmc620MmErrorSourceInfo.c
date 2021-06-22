/** @file
  Create and populate DMC-620 HEST error source descriptors.

  Implements the HEST Error Source Descriptor protocol. Creates the GHESv2
  type error source descriptors for supported hardware errors. Appends
  the created descriptors to the Buffer parameter of the protocol.

  Copyright (c) 2020 - 2021, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
    - ACPI Reference Specification 6.3, Table 18-393 GHESv2 Structure.
**/

#include <Dmc620Mm.h>
#include <HestAcpiHeader.h>

/**
  Populate the DMC-620 DRAM Error Source Descriptor.

  Creates error source descriptor of GHESv2 type to be appended to the Hest
  table. The error source descriptor is populated with appropriate values
  based on the instance number of DMC-620. Allocates and initializes memory
  for Error Status Block(Cper) section for each error source.

  @param[in]  ErrorDesc  HEST error source descriptor Information.
  @param[in]  DmcIdx     Instance number of the DMC-620.
**/
STATIC
VOID
EFIAPI
Dmc620SetupDramErrorDescriptor (
  IN  EFI_ACPI_6_3_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE *ErrorDesc,
  IN  UINTN     DmcIdx
  )
{
  UINTN  ErrorBlockData;

  //
  // Address of reserved memory for the error status block that will be used
  // to hold the information about the DRAM error. Initialize this memory
  // with 0.
  //
  ErrorBlockData = FixedPcdGet64 (PcdDmc620DramOneBitErrorDataBase) +
                     (FixedPcdGet64 (PcdDmc620DramOneBitErrorDataSize) *
                      DmcIdx);
  SetMem (
    (VOID *)ErrorBlockData,
    FixedPcdGet64 (PcdDmc620DramOneBitErrorDataSize),
    0
    );

  // Build the DRAM error source descriptor.
  *ErrorDesc =
    (EFI_ACPI_6_3_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE) {
      .Type = EFI_ACPI_6_3_GENERIC_HARDWARE_ERROR_VERSION_2,
      .SourceId = FixedPcdGet16 (PcdDmc620DramOneBitErrorSourceId) + DmcIdx,
      .RelatedSourceId = 0xFFFF,
      .Flags = 0,
      .Enabled = 1,
      .NumberOfRecordsToPreAllocate = 1,
      .MaxSectionsPerRecord = 1,
      .MaxRawDataLength = sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA),
      .ErrorStatusAddress =
        EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE_INIT (
          (ErrorBlockData + 8)
          ),
      .NotificationStructure =
        EFI_ACPI_6_3_HARDWARE_ERROR_NOTIFICATION_STRUCTURE_INIT (
          EFI_ACPI_6_3_HARDWARE_ERROR_NOTIFICATION_SOFTWARE_DELEGATED_EXCEPTION,
          0,
          FixedPcdGet32 (PcdDmc620DramErrorSdeiEventBase) + DmcIdx
          ),
      .ErrorStatusBlockLength =
        sizeof (EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE) +
        sizeof (EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE) +
        sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA),
      .ReadAckRegister =
        EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE_INIT (ErrorBlockData),
      .ReadAckPreserve = 0,
      .ReadAckWrite = 0
      };
}

/**
  MMI handler implementing the HEST error source descriptor protocol.

  Returns the error source descriptor information for all supported hardware
  error sources. As mentioned in the HEST Error Source Decriptor protocol this
  handler returns with error source count and length when Buffer parameter is
  NULL.

  @param[in]   This                Pointer for this protocol.
  @param[out]  Buffer              HEST error source descriptor Information
                                   buffer.
  @param[out]  ErrorSourcesLength  Total length of Error Source Descriptors
  @param[out]  ErrorSourceCount    Total number of supported error spurces.

  @retval  EFI_SUCCESS            Buffer has valid Error Source descriptor
                                  information.
  @retval  EFI_INVALID_PARAMETER  Buffer is NULL.
**/
STATIC
EFI_STATUS
EFIAPI
Dmc620ErrorSourceDescInfoGet (
  IN  MM_HEST_ERROR_SOURCE_DESC_PROTOCOL *This,
  OUT VOID                               **Buffer,
  OUT UINTN                              *ErrorSourcesLength,
  OUT UINTN                              *ErrorSourcesCount
  )
{
  EFI_ACPI_6_3_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE *ErrorDescriptor;
  UINTN                                                          DmcIdx;

  //
  // Update the error source length and error source count parameters.
  //
  *ErrorSourcesLength =
    FixedPcdGet64 (PcdDmc620NumCtrl) *
    FixedPcdGet64 (PcdDmc620ErrSourceCount) *
    sizeof(EFI_ACPI_6_3_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE);
  *ErrorSourcesCount = FixedPcdGet64 (PcdDmc620NumCtrl) *
                         FixedPcdGet64 (PcdDmc620ErrSourceCount);

  //
  // If 'Buffer' is NULL return, as this invocation of the protocol handler is
  // to determine the total size of all the error source descriptor instances.
  //
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Buffer to be updated with error source descriptor(s) information.
  ErrorDescriptor =
    (EFI_ACPI_6_3_GENERIC_HARDWARE_ERROR_SOURCE_VERSION_2_STRUCTURE *)*Buffer;

  //
  // Create and populate the available error source descriptor for all DMC(s).
  //
  for (DmcIdx = 0; DmcIdx < FixedPcdGet64 (PcdDmc620NumCtrl); DmcIdx++) {
    // Add the one-bit DRAM error source descriptor.
    Dmc620SetupDramErrorDescriptor(ErrorDescriptor, DmcIdx);
    ErrorDescriptor++;
  }

  return EFI_SUCCESS;
}

//
// DMC-620 MM_HEST_ERROR_SOURCE_DESC_PROTOCOL protocol instance.
//
STATIC MM_HEST_ERROR_SOURCE_DESC_PROTOCOL mDmc620ErrorSourceDesc = {
  Dmc620ErrorSourceDescInfoGet
};

/**
  Allow reporting of supported DMC-620 error sources.

  Install the HEST Error Source Descriptor protocol handler to allow publishing
  of the supported Dmc(s) hardware error sources.

  @param[in]  MmSystemTable  Pointer to System table.

  @retval  EFI_SUCCESS            Protocol installation successful.
  @retval  EFI_INVALID_PARAMETER  Invalid system table parameter.
**/
EFI_STATUS
Dmc620InstallErrorSourceDescProtocol (
  IN EFI_MM_SYSTEM_TABLE *MmSystemTable
  )
{
  EFI_HANDLE mDmcHandle = NULL;
  EFI_STATUS Status;

  // Check if the MmSystemTable is initialized.
  if (MmSystemTable == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Install HEST error source descriptor protocol for DMC(s).
  Status = MmSystemTable->MmInstallProtocolInterface (
                            &mDmcHandle,
                            &gMmHestErrorSourceDescProtocolGuid,
                            EFI_NATIVE_INTERFACE,
                            &mDmc620ErrorSourceDesc
                            );
  if (EFI_ERROR(Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed installing HEST error source protocol, status: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  return Status;
}
