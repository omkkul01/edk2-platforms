/** @file
  DMC-620 memory controller MM driver definitions.

  Macros and structure definitions for DMC-620 error handling MM driver.

  Copyright (c) 2020 - 2021, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef DMC620_MM_DRIVER_H_
#define DMC620_MM_DRIVER_H_

#include <Base.h>
#include <Guid/Cper.h>
#include <IndustryStandard/Acpi.h>
#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Protocol/HestErrorSourceInfo.h>

// DMC-620 memc register field values and masks.
#define DMC620_MEMC_STATUS_MASK       (BIT2|BIT1|BIT0)
#define DMC620_MEMC_STATUS_READY      (BIT1|BIT0)
#define DMC620_MEMC_CMD_EXECUTE_DRAIN (BIT2|BIT0)

// DMC-620 Error Record Status register fields values and masks.
#define DMC620_ERR_STATUS_MV BIT26
#define DMC620_ERR_STATUS_AV BIT31

// DMC-620 Error Record MISC-0 register fields values and masks.
#define DMC620_ERR_MISC0_COLUMN_MASK \
  (BIT9|BIT8|BIT7|BIT6|BIT5|BIT4|BIT3|BIT2|BIT1|BIT0)
#define DMC620_ERR_MISC0_ROW_MASK   (0x0FFFFC00)
#define DMC620_ERR_MISC0_ROW_SHIFT  10
#define DMC620_ERR_MISC0_RANK_MASK  (BIT30|BIT29|BIT28)
#define DMC620_ERR_MISC0_RANK_SHIFT 28
#define DMC620_ERR_MISC0_VAILD      BIT31

// DMC-620 Error Record register fields values and mask.
#define DMC620_ERR_MISC1_VAILD     BIT31
#define DMC620_ERR_MISC1_BANK_MASK (BIT3|BIT2|BIT1|BIT0)

// DMC-620 Error Record Global Status register bit field.
#define DMC620_ERR_GSR_ECC_CORRECTED_FH BIT1

//
// DMC-620 Memory Mapped register definitions.
//

// Unused DMC-620 register fields.
#define RESV_0 0x1BD
#define RESV_1 0x2C
#define RESV_2 0x8
#define RESV_3 0x58

#pragma pack(1)
typedef struct {
  UINT32 MemcStatus;
  UINT32 MemcConfig;
  UINT32 MemcCmd;
  UINT32 Reserved[RESV_0];
  UINT32 Err0Fr;
  UINT32 Reserved1;
  UINT32 Err0Ctlr0;
  UINT32 Err0Ctlr1;
  UINT32 Err0Status;
  UINT8  Reserved2[RESV_1];
  UINT32 Err1Fr;
  UINT32 Reserved3;
  UINT32 Err1Ctlr;
  UINT32 Reserved4;
  UINT32 Err1Status;
  UINT32 Reserved5;
  UINT32 Err1Addr0;
  UINT32 Err1Addr1;
  UINT32 Err1Misc0;
  UINT32 Err1Misc1;
  UINT32 Err1Misc2;
  UINT32 Err1Misc3;
  UINT32 Err1Misc4;
  UINT32 Err1Misc5;
  UINT8  Reserved6[RESV_2];
  UINT32 Err2Fr;
  UINT32 Reserved7;
  UINT32 Err2Ctlr;
  UINT32 Reserved8;
  UINT32 Err2Status;
  UINT32 Reserved9;
  UINT32 Err2Addr0;
  UINT32 Err2Addr1;
  UINT32 Err2Misc0;
  UINT32 Err2Misc1;
  UINT32 Err2Misc2;
  UINT32 Err2Misc3;
  UINT32 Err2Misc4;
  UINT32 Err2Misc5;
  UINT8  Reserved10[RESV_2];
  UINT32 Reserved11[RESV_3];
  UINT32 Errgsr;
} DMC620_REGS_TYPE;

// DMC-620 Typical Error Record register definition.
typedef struct {
  UINT32 ErrFr;
  UINT32 Reserved;
  UINT32 ErrCtlr;
  UINT32 Reserved1;
  UINT32 ErrStatus;
  UINT32 Reserved2;
  UINT32 ErrAddr0;
  UINT32 ErrAddr1;
  UINT32 ErrMisc0;
  UINT32 ErrMisc1;
  UINT32 ErrMisc2;
  UINT32 ErrMisc3;
  UINT32 ErrMisc4;
  UINT32 ErrMisc5;
  UINT8  Reserved3[RESV_2];
} DMC620_ERR_REGS_TYPE;
#pragma pack()

// List of supported error sources by DMC-620.
typedef enum {
  DramEccCfh = 0,
  DramEccFh,
  ChiFh,
  SramEccCfh,
  SramEccFh,
  DmcErrRecovery
} DMC_ERR_SOURCES;

/**
  MMI handler implementing the HEST error source desc protocol.

  Returns the error source descriptor information for all DMC(s) error sources
  and also returns its count and length.

  @param[in]   This                Pointer for this protocol.

  @param[out]  Buffer              HEST error source descriptor Information
                                   buffer.
  @param[out]  ErrorSourcesLength  Total length of Error Source Descriptors.
  @param[out]  ErrorSourceCount    Total number of supported error sources.

  @retval  EFI_SUCCESS            Buffer has valid Error Source descriptor
                                  information.
  @retval  EFI_INVALID_PARAMETER  Buffer is NULL.
**/
EFI_STATUS
EFIAPI
DmcErrorSourceDescInfoGet (
  IN  MM_HEST_ERROR_SOURCE_DESC_PROTOCOL *This,
  OUT VOID                               **Buffer,
  OUT UINTN                              *ErrorSourcesLength,
  OUT UINTN                              *ErrorSourcesCount
  );

/**
  Allow reporting of supported DMC-620 error sources.

  Install the HEST Error Source Descriptor protocol handler to allow publishing
  of the supported DMC-620 memory controller error sources.

  @param[in]  MmSystemTable  Pointer to System table.

  @retval  EFI_SUCCESS            Protocol installation successful.
  @retval  EFI_INVALID_PARAMETER  Invalid system table parameter.
**/
EFI_STATUS
Dmc620InstallErrorSourceDescProtocol (
  IN EFI_MM_SYSTEM_TABLE *MmSystemTable
  );

#endif // DMC620_MM_DRIVER_H_
