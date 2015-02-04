BlockSize          = 0x1000
FvBaseAddress      = 0x80000400
FvForceRebase      = TRUE
FvAlignment        = 16         #FV alignment and FV attributes setting.
ERASE_POLARITY     = 1
MEMORY_MAPPED      = TRUE
STICKY_WRITE       = TRUE
LOCK_CAP           = TRUE
LOCK_STATUS        = TRUE
WRITE_DISABLED_CAP = TRUE
WRITE_ENABLED_CAP  = TRUE
WRITE_STATUS       = TRUE
WRITE_LOCK_CAP     = TRUE
WRITE_LOCK_STATUS  = TRUE
READ_DISABLED_CAP  = TRUE
READ_ENABLED_CAP   = TRUE
READ_STATUS        = TRUE
READ_LOCK_CAP      = TRUE
READ_LOCK_STATUS   = TRUE
FvNameGuid         = 18D6D9F4-2EEF-4913-AEE6-BE61C6DA6CC8

################################################################################
#
# The INF statements point to EDK component and EDK II module INF files, which will be placed into this FV image.
# Parsing tools will scan the INF file to determine the type of component or module.
# The component or module type is used to reference the standard rules
# defined elsewhere in the FDF file.
#
# The format for INF statements is:
# INF $(PathAndInfFileName)
#
################################################################################
  ##
  #  PEI Apriori file example, more PEIM module added later.
  ##
APRIORI PEI {
  INF  MdeModulePkg/Universal/PCD/Pei/Pcd.inf
  # PlatformConfigPei should be immediately after Pcd driver.
  INF  QuarkPlatformPkg/Platform/Pei/PlatformConfig/PlatformConfigPei.inf
  INF  MdeModulePkg/Universal/PcatSingleSegmentPciCfg2Pei/PcatSingleSegmentPciCfg2Pei.inf
  INF  MdeModulePkg/Universal/ReportStatusCodeRouter/Pei/ReportStatusCodeRouterPei.inf
  INF  MdeModulePkg/Universal/StatusCodeHandler/Pei/StatusCodeHandlerPei.inf
  INF  QuarkPlatformPkg/Platform/Pei/PlatformInfo/PlatformInfo.inf
  }

  ##
  #  PEI Phase RAW Data files.
  ##
FILE FREEFORM = PCD(gEfiQuarkNcSocIdTokenSpaceGuid.PcdQuarkMicrocodeFile) {
!ifdef QUARK2
    SECTION RAW = QuarkSocPkg/QuarkNorthCluster/Binary/Quark2Microcode/RMU.bin
!else
    SECTION RAW = QuarkSocPkg/QuarkNorthCluster/Binary/QuarkMicrocode/RMU.bin
!endif
  }

  ##
  #  Platform data files see QuarkPlatformPkg/Include/Guid/PlatformDataFileNameGuids.h
  #  for steps to add new platform.
  ##
FILE FREEFORM = 0A975562-DF47-4dc3-8AB0-3BA2C3522302 {
    SECTION RAW = QuarkPlatformPkg/Binary/PlatformData/svp-platform-data.bin
  }
FILE FREEFORM = 956EDAD3-8440-45cb-89AC-D1930C004E34 {
    SECTION RAW = QuarkPlatformPkg/Binary/PlatformData/kipsbay-platform-data.bin
  }
FILE FREEFORM = 095B3C16-6D67-4c85-B528-339D9FF6222C {
    SECTION RAW = QuarkPlatformPkg/Binary/PlatformData/crosshill-platform-data.bin
  }
FILE FREEFORM = EE84C5E7-9412-42cc-B755-A915A7B68536 {
    SECTION RAW = QuarkPlatformPkg/Binary/PlatformData/clantonhill-platform-data.bin
  }
FILE FREEFORM = E4AD87C8-D20E-40ce-97F5-9756FD0E81D4 {
    SECTION RAW = QuarkPlatformPkg/Binary/PlatformData/galileo-platform-data.bin
  }
FILE FREEFORM = 23B3C10D-46E3-4a78-8AAA-217B6A39EF04 {
    SECTION RAW = QuarkPlatformPkg/Binary/PlatformData/galileo-gen2-platform-data.bin
  }


  ##
  #  PEI Phase modules
  ##
INF  IA32FamilyCpuBasePkg/SecCore/SecCore.inf
INF  MdeModulePkg/Core/Pei/PeiMain.inf
INF  RuleOverride = NORELOC  MdeModulePkg/Universal/PCD/Pei/Pcd.inf
# PlatformConfigPei should be immediately after Pcd driver.
INF  QuarkPlatformPkg/Platform/Pei/PlatformConfig/PlatformConfigPei.inf
INF  RuleOverride = NORELOC  MdeModulePkg/Universal/ReportStatusCodeRouter/Pei/ReportStatusCodeRouterPei.inf
INF  RuleOverride = NORELOC  MdeModulePkg/Universal/StatusCodeHandler/Pei/StatusCodeHandlerPei.inf
!ifdef SECURE_BOOT
  INF  RuleOverride = NORELOC  SecurityPkg/VariableAuthenticated/Pei/VariablePei.inf
!else
  INF  RuleOverride = NORELOC  MdeModulePkg/Universal/Variable/Pei/VariablePei.inf
!endif
INF  RuleOverride = NORELOC  IA32FamilyCpuBasePkg/CpuPei/CpuPei.inf
INF  RuleOverride = NORELOC  MdeModulePkg/Universal/CapsulePei/CapsulePei.inf
INF  RuleOverride = NORELOC  QuarkSocPkg/QuarkNorthCluster/MemoryInit/Pei/MemoryInitPei.inf
INF  RuleOverride = NORELOC  QuarkSocPkg/QuarkNorthCluster/Smm/Pei/SmmAccessPei/SmmAccessPei.inf
INF  RuleOverride = NORELOC  QuarkSocPkg/QuarkNorthCluster/Smm/Pei/SmmControlPei/SmmControlPei.inf
INF  QuarkPlatformPkg/Platform/Pei/PlatformInit/PlatformEarlyInit.inf
INF  QuarkPlatformPkg/Platform/Pei/PlatformInfo/PlatformInfo.inf
INF  MdeModulePkg/Universal/PcatSingleSegmentPciCfg2Pei/PcatSingleSegmentPciCfg2Pei.inf
INF  QuarkPlatformPkg/Override/MdeModulePkg/Core/DxeIplPeim/DxeIpl.inf
