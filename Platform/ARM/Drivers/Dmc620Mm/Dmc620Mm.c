/** @file
  DMC-620 Memory Controller error handling (Standalone MM) driver.

  Supports 1-bit Bit DRAM error handling for multiple DMC instances. On a error
  event, publishes the CPER error record of Memory Error type.

  Copyright (c) 2020 - 2021, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference
    - DMC620 Dynamic Memory Controller, revision r1p0.
    - UEFI Reference Specification 2.8, Section N.2.5 Memory Error Section
**/

#include <Dmc620Mm.h>

/**
  Helper function to handle the DMC-620 DRAM errors.

  Reads the DRAM error record registers. Creates a CPER error record of type
  'Memory Error' and populates it with information collected from DRAM error
  record registers.

  @param[in]  DmcCtrl                A pointer to DMC control registers.
  @param[in]  DmcInstance            DMC instance which raised the fault event.
  @param[in]  ErrRecType             A type of the DMC error record.
  @param[in]  ErrorBlockBaseAddress  Unique address for populating the error
                                     block status for given DMC error source.
**/
STATIC
VOID
Dmc620HandleDramError (
  IN DMC620_REGS_TYPE *DmcCtrl,
  IN UINTN            DmcInstance,
  IN UINTN            ErrRecType,
  IN UINTN            ErrorBlockBaseAddress
  )
{
  EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE *ErrBlockSectionDesc;
  EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE     *ErrBlockStatusHeaderData;
  EFI_PLATFORM_MEMORY_ERROR_DATA                  MemorySectionInfo = {0};
  DMC620_ERR_REGS_TYPE                            *ErrRecord;
  EFI_GUID                                        SectionType;
  UINT32                                          ResetReg;
  VOID                                            *ErrBlockSectionData;
  UINTN                                           *ErrorStatusRegister;
  UINTN                                           *ReadAckRegister;
  UINTN                                           *ErrStatusBlock;
  UINTN                                           ErrStatus;
  UINTN                                           ErrAddr0;
  UINTN                                           ErrAddr1;
  UINTN                                           ErrMisc0;
  UINTN                                           ErrMisc1;
  UINT8                                           CorrectedError;

  //
  // Check the type of DRAM error (1-bit or 2-bit) and accordingly select
  // error record to use.
  //
  if (ErrRecType == DMC620_ERR_GSR_ECC_CORRECTED_FH) {
    DEBUG ((
      DEBUG_INFO,
      "%a: DRAM ECC Corrected Fault (1-Bit ECC error)\n",
      __FUNCTION__
      ));
    ErrRecord = (DMC620_ERR_REGS_TYPE *)&DmcCtrl->Err1Fr;
    CorrectedError = 1;
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%a: DRAM ECC Fault Handling (2-bit ECC error)\n",
      __FUNCTION__
      ));
    ErrRecord = (DMC620_ERR_REGS_TYPE *)&DmcCtrl->Err2Fr;
    CorrectedError = 0;
  }

  // Read most recent DRAM error record registers.
  ErrStatus = MmioRead32 ((UINTN)&ErrRecord->ErrStatus);
  ErrAddr0  = MmioRead32 ((UINTN)&ErrRecord->ErrAddr0);
  ErrAddr1  = MmioRead32 ((UINTN)&ErrRecord->ErrAddr1);
  ErrMisc0  = MmioRead32 ((UINTN)&ErrRecord->ErrMisc0);
  ErrMisc1  = MmioRead32 ((UINTN)&ErrRecord->ErrMisc1);

  // Clear the status register so that new error records are populated.
  ResetReg = MmioRead32 ((UINTN)&ErrRecord->ErrStatus);
  MmioWrite32 ((UINTN)&ErrRecord->ErrStatus, ResetReg);

  //
  // Get Physical address of DRAM error from Error Record Address register
  // and populate Memory Error Section.
  //
  if (ErrStatus & DMC620_ERR_STATUS_AV) {
    DEBUG ((
      DEBUG_INFO,
      "%a: DRAM Error: Address_0 : 0x%x Address_1 : 0x%x\n",
      __FUNCTION__,
      ErrAddr0,
      ErrAddr1
      ));

    //
    // Populate Memory CPER section with DRAM error address (48 bits) and
    // address mask fields.
    //
    MemorySectionInfo.ValidFields |=
      EFI_PLATFORM_MEMORY_PHY_ADDRESS_MASK_VALID |
      EFI_PLATFORM_MEMORY_PHY_ADDRESS_VALID;
    MemorySectionInfo.PhysicalAddressMask = 0xFFFFFFFFFFFF;
    MemorySectionInfo.PhysicalAddress = (ErrAddr1 << 32) | ErrAddr0;
  }

  //
  // Read the Error Record Misc registers and populate relevant fields in
  // Memory CPER error section.
  //
  if ((ErrStatus & DMC620_ERR_STATUS_MV)
      && (ErrMisc0 & DMC620_ERR_MISC0_VAILD))
  {
    // Populate Memory error section wih DRAM column information.
    MemorySectionInfo.ValidFields |= EFI_PLATFORM_MEMORY_COLUMN_VALID;
    MemorySectionInfo.Column = ErrMisc0 & DMC620_ERR_MISC0_COLUMN_MASK;

    //
    // Populate Memory Error Section with DRAM row information.
    // Row bits (bit 16 and 17) are to be filled as extended.
    //
    MemorySectionInfo.ValidFields |=
      EFI_PLATFORM_MEMORY_ERROR_EXTENDED_ROW_BIT_16_17_VALID;
    MemorySectionInfo.Row =
      (ErrMisc0 & DMC620_ERR_MISC0_ROW_MASK) >> DMC620_ERR_MISC0_ROW_SHIFT;
    MemorySectionInfo.Extended =
      ((ErrMisc0 & DMC620_ERR_MISC0_ROW_MASK) >>
       (DMC620_ERR_MISC0_ROW_SHIFT + 16));

    // Populate Memory Error Section wih DRAM rank information.
    MemorySectionInfo.ValidFields |= EFI_PLATFORM_MEMORY_ERROR_RANK_NUM_VALID;
    MemorySectionInfo.RankNum = (ErrMisc0 & DMC620_ERR_MISC0_RANK_MASK) >>
      DMC620_ERR_MISC0_RANK_SHIFT;
  }

  // Read Error Record MISC1 register and populate the Memory Error Section.
  if ((ErrStatus & DMC620_ERR_STATUS_MV)
      && (ErrMisc1 & DMC620_ERR_MISC1_VAILD))
  {
    MemorySectionInfo.ValidFields |= EFI_PLATFORM_MEMORY_BANK_VALID;
    MemorySectionInfo.Bank = (ErrMisc1 & DMC620_ERR_MISC1_BANK_MASK);
  }

  //
  // Misc registers 2..5 are not used and convey only the error counter
  // information. They are cleared as they do not contribute in Error
  // Record creation.
  //
  if (ErrStatus & DMC620_ERR_STATUS_MV) {
    ResetReg = 0x0;
    MmioWrite32 ((UINTN)&ErrRecord->ErrMisc2, ResetReg);
    MmioWrite32 ((UINTN)&ErrRecord->ErrMisc3, ResetReg);
    MmioWrite32 ((UINTN)&ErrRecord->ErrMisc4, ResetReg);
    MmioWrite32 ((UINTN)&ErrRecord->ErrMisc5, ResetReg);
  }

  //
  // Reset error records Status register for recording new DRAM error syndrome
  // information.
  //
  ResetReg = MmioRead32 ((UINTN)&ErrRecord->ErrStatus);
  MmioWrite32 ((UINTN)&ErrRecord->ErrStatus, ResetReg);

  //
  // Allocate memory for Error Acknowledge register, Error Status register and
  // Error status block data.
  //
  ReadAckRegister = (UINTN *)ErrorBlockBaseAddress;
  ErrorStatusRegister = (UINTN *)ErrorBlockBaseAddress + 1;
  ErrStatusBlock = (UINTN *)ErrorStatusRegister + 1;

  // Initialize Error Status Register with Error Status Block address.
  *ErrorStatusRegister = (UINTN)ErrStatusBlock;

  //
  // Locate Block Status Header base address and populate it with Error Status
  // Block Header information.
  //
  ErrBlockStatusHeaderData = (EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE *)
                             ErrStatusBlock;
  *ErrBlockStatusHeaderData =
    (EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE) {
      .BlockStatus = {
        .UncorrectableErrorValid     = ((CorrectedError == 0) ? 0:1),
        .CorrectableErrorValid       = ((CorrectedError == 1) ? 1:0),
        .MultipleUncorrectableErrors = 0x0,
        .MultipleCorrectableErrors   = 0x0,
        .ErrorDataEntryCount         = 0x1
       },
      .RawDataOffset = (sizeof(EFI_ACPI_6_3_GENERIC_ERROR_STATUS_STRUCTURE) +
                       sizeof(EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE)),
      .RawDataLength = 0,
      .DataLength = (sizeof(EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE) +
                    sizeof(EFI_PLATFORM_MEMORY_ERROR_DATA)),
      .ErrorSeverity = ((CorrectedError == 1) ?
                         EFI_ACPI_6_3_ERROR_SEVERITY_CORRECTED:
                         EFI_ACPI_6_3_ERROR_SEVERITY_FATAL),
    };

  //
  // Locate Section Descriptor base address and populate Error Status Section
  // Descriptor data.
  //
  ErrBlockSectionDesc = (EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE *)
                        (ErrBlockStatusHeaderData + 1);
  *ErrBlockSectionDesc =
    (EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_STRUCTURE) {
      .ErrorSeverity = ((CorrectedError == 1) ?
                         EFI_ACPI_6_3_ERROR_SEVERITY_CORRECTED:
                         EFI_ACPI_6_3_ERROR_SEVERITY_FATAL),
      .Revision = EFI_ACPI_6_3_GENERIC_ERROR_DATA_ENTRY_REVISION,
      .ValidationBits = 0,
      .Flags = 0,
      .ErrorDataLength = sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA),
      .FruId = {0},
      .FruText = {0},
      .Timestamp = {0},
    };
  SectionType = (EFI_GUID) EFI_ERROR_SECTION_PLATFORM_MEMORY_GUID;
  CopyGuid ((EFI_GUID *)ErrBlockSectionDesc->SectionType, &SectionType);

  // Locate Section base address and populate Memory Error Section(Cper) data.
  ErrBlockSectionData = (VOID *)(ErrBlockSectionDesc + 1);
  CopyMem (
    ErrBlockSectionData,
    (VOID *)&MemorySectionInfo,
    sizeof (EFI_PLATFORM_MEMORY_ERROR_DATA)
    );
}

/**
  DMC-620 1-bit ECC event handler.

  Supports multiple DMC error processing. Current implementation handles the
  DRAM ECC errors.

  @param[in]  DispatchHandle       The unique handle assigned to this handler by
                                   MmiHandlerRegister().
  @param[in]  Context              Points to an optional handler context which
                                   was specified when the handler was
                                   registered.
  @param[in, out]  CommBuffer      Buffer passed from Non-MM to MM environmvent.
  @param[in, out]  CommBufferSize  The size of the CommBuffer.

  @retval  EFI_SUCCESS  Event handler successful.
  @retval  Other        Failure of event handler.
**/
STATIC
EFI_STATUS
EFIAPI
Dmc620ErrorEventHandler (
  IN     EFI_HANDLE DispatchHandle,
  IN     CONST VOID *Context,       OPTIONAL
  IN OUT VOID       *CommBuffer,    OPTIONAL
  IN OUT UINTN      *CommBufferSize OPTIONAL
  )
{
  DMC620_REGS_TYPE *DmcCtrl;
  UINTN            DmcIdx;
  UINTN            ErrGsr;

  // DMC instance which raised the error event.
  DmcIdx = *(UINTN *)CommBuffer;
  // Error Record Base address for that DMC instance.
  DmcCtrl = (DMC620_REGS_TYPE *)(FixedPcdGet64 (PcdDmc620RegisterBase) +
            (FixedPcdGet64 (PcdDmc620CtrlSize) * DmcIdx));

  DEBUG ((
    DEBUG_INFO,
    "%a: DMC error event raised for DMC: %d with DmcBaseAddr: 0x%x \n",
    __FUNCTION__,
    DmcIdx,
    (UINTN)DmcCtrl
    ));

  ErrGsr = MmioRead32 ((UINTN)&DmcCtrl->Errgsr);

  if (ErrGsr & DMC620_ERR_GSR_ECC_CORRECTED_FH) {
    // Handle corrected 1-bit DRAM ECC error.
    Dmc620HandleDramError (
      DmcCtrl,
      DmcIdx,
      DMC620_ERR_GSR_ECC_CORRECTED_FH,
      FixedPcdGet64 (
        PcdDmc620DramOneBitErrorDataBase) +
        (FixedPcdGet64 (PcdDmc620DramOneBitErrorDataSize) * DmcIdx)
        );
  } else {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Unsupported DMC-620 error reported, ignoring\n",
      __FUNCTION__
      ));
  }

  // No data to send using the MM communication buffer so clear the comm buffer
  // size.
  *CommBufferSize = 0;

  return EFI_SUCCESS;
}

/**
  Initialize function for the driver.

  Registers MMI handlers to process fault events on DMC and installs required
  protocols to publish the error source descriptors.

  @param[in]  ImageHandle  Handle to image.
  @param[in]  SystemTable  Pointer to System table.

  @retval  EFI_SUCCESS  On successful installation of error event handler for
                        DMC.
  @retval  Other        Failure in installing error event handlers for DMC.
**/
EFI_STATUS
EFIAPI
Dmc620MmDriverInitialize (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_MM_SYSTEM_TABLE *SystemTable
  )
{
  EFI_MM_SYSTEM_TABLE *mMmst;
  EFI_STATUS          Status;
  EFI_HANDLE          DispatchHandle;

  ASSERT (SystemTable != NULL);
  mMmst = SystemTable;

  // Register MMI handlers for DMC-620 error events.
  Status = mMmst->MmiHandlerRegister (
                    Dmc620ErrorEventHandler,
                    &gArmDmcEventHandlerGuid,
                    &DispatchHandle
                     );
  if (EFI_ERROR(Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Registration failed for DMC error event handler, Status:%r\n",
      __FUNCTION__,
      Status
      ));

     return Status;
  }

  // Installs the HEST error source descriptor protocol.
  Status = Dmc620InstallErrorSourceDescProtocol (SystemTable);
  if (EFI_ERROR(Status)) {
    mMmst->MmiHandlerUnRegister (DispatchHandle);
  }

  return Status;
}
