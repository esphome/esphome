# ESPHome matter External Platform manifest.
#
# esp-matter's own CMakeLists.txt runs this file when
# CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM=y (set from esphome/components/matter/
# __init__.py). It replaces the "else()" branch that would otherwise fold
# ${MATTER_SDK_PATH}/src/platform/ESP32 into the build with a hard-coded set
# of includes/excludes.
#
# The conditional block below mirrors the ACTUAL else() branch of esp-matter
# 1.6+ verbatim — not the blemesh_platform reference, which is a
# subset (blemesh omits the OT-endpoint arms and the memory-allocation-mode
# arms). Layer one more unconditional exclusion on top:
# NetworkCommissioningDriver_Ethernet.cpp, whose Init() hardcodes the IP101
# PHY on the ESP32 EMAC and fights with ESPHome's ethernet: component. Its
# replacement (a no-op Init that returns CHIP_NO_ERROR — ESPHome already
# owns the netif) lives in ../matter_ethernet_stub.cpp.
#
# MATTER_SDK_PATH is set at line 1 of esp_matter's CMakeLists.txt (points at
# managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/)
# and is in scope by the time this file is include()d.

# ---- Generate the guarded BuildConfig headers -----------------------------
# Four of esp_matter's managed_component_include/*BuildConfig.cmake files
# guard their file(WRITE ...) with ``if (NOT CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM)``,
# under the assumption that an external platform would supply its own copies
# (blemesh_platform ships them as symlinks). We don't rename or replace the
# ESP32 platform files, so re-run each generator with the gate temporarily
# turned off — the headers land in ${CMAKE_CURRENT_BINARY_DIR}/platform/,
# which is already on INCLUDE_DIRS_LIST from esp_matter's CMakeLists. Without
# this, CHIP source that ``#include <platform/CHIPDeviceBuildConfig.h>``
# (and BleConfig / InetConfig / SystemConfig peers) fails to compile.
get_filename_component(_esphome_matter_esp_matter_root "${MATTER_SDK_PATH}/../.." REALPATH)
set(_esphome_matter_saved_external_platform ${CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM})
set(CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM OFF)
foreach(_esphome_matter_buildconfig
        CHIPDeviceBuildConfig
        BleBuildConfig
        InetBuildConfig
        SystemBuildConfig)
    include("${_esphome_matter_esp_matter_root}/managed_component_include/${_esphome_matter_buildconfig}.cmake")
endforeach()
set(CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM ${_esphome_matter_saved_external_platform})

# NOTE: the *BuildConfig.h headers we just regenerated still wrap their
# platform-include defines in an inner ``#ifndef
# CONFIG_CHIP_ENABLE_EXTERNAL_PLATFORM`` at C preprocessor time, and
# sdkconfig.h defines that macro (we set it =y in matter/__init__.py). The
# compensating CHIP_*_PLATFORM_CONFIG_INCLUDE + CHIP_DEVICE_LAYER_TARGET
# macros are supplied via ``cg.add_build_flag`` in ../__init__.py so they
# reach the compile line BEFORE the guarded #ifndef blocks are evaluated,
# leaving the generated headers unmodified.

# ---- Source directories ---------------------------------------------------
set(EXPLANT_SRC_DIRS_LIST "${MATTER_SDK_PATH}/src/platform/ESP32"
                          "${MATTER_SDK_PATH}/src/platform/ESP32/route_hook")

set(EXPLANT_INCLUDE_DIRS_LIST "${MATTER_SDK_PATH}/src/platform/ESP32"
                              "${MATTER_SDK_PATH}/src/platform/ESP32/bluedroid"
                              "${MATTER_SDK_PATH}/src/platform/ESP32/nimble"
                              "${MATTER_SDK_PATH}/src/platform/ESP32/route_hook")

set(EXPLANT_EXCLUDE_SRCS_LIST)

# ---- ESPHome unconditional exclusion --------------------------------------
# Owned by matter_ethernet_stub.cpp on the ESPHome side (see file header).
list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
    "${MATTER_SDK_PATH}/src/platform/ESP32/NetworkCommissioningDriver_Ethernet.cpp")

# ---- Conditional exclusions mirroring esp-matter's else() branch ----------
# Keep aligned with managed_components/espressif__esp_matter/CMakeLists.txt.
# When Espressif adds/moves an exclusion arm there, mirror it here.

if(CONFIG_USE_MINIMAL_MDNS)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/DnssdImpl.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32DnssdImpl.cpp")
endif()

if((NOT CONFIG_ENABLE_WIFI_STATION) AND (NOT CONFIG_ENABLE_WIFI_AP))
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/ConnectivityManagerImpl_WiFi.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/NetworkCommissioningDriver.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/route_hook/ESP32RouteHook.c"
        "${MATTER_SDK_PATH}/src/platform/ESP32/route_hook/ESP32RouteTable.c")
endif()

if(NOT CONFIG_ENABLE_ROUTE_HOOK)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/route_hook/ESP32RouteHook.c"
        "${MATTER_SDK_PATH}/src/platform/ESP32/route_hook/ESP32RouteTable.c")
endif()

if((CONFIG_BT_ENABLED) AND (CONFIG_ENABLE_CHIPOBLE))
    if(CONFIG_BT_NIMBLE_ENABLED)
        list(APPEND EXPLANT_SRC_DIRS_LIST
            "${MATTER_SDK_PATH}/src/platform/ESP32/nimble")
        if(NOT CONFIG_ENABLE_ESP32_BLE_CONTROLLER)
            list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
                "${MATTER_SDK_PATH}/src/platform/ESP32/nimble/ChipDeviceScanner.cpp"
                "${MATTER_SDK_PATH}/src/platform/ESP32/nimble/misc.c"
                "${MATTER_SDK_PATH}/src/platform/ESP32/nimble/peer.c")
        endif()
    else()
        list(APPEND EXPLANT_SRC_DIRS_LIST
            "${MATTER_SDK_PATH}/src/platform/ESP32/bluedroid")
        if(NOT CONFIG_ENABLE_ESP32_BLE_CONTROLLER)
            list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
                "${MATTER_SDK_PATH}/src/platform/ESP32/bluedroid/ChipDeviceScanner.cpp")
        endif()
    endif()
endif()

if(NOT CONFIG_ENABLE_ETHERNET_TELEMETRY)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/ConnectivityManagerImpl_Ethernet.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/NetworkCommissioningDriver_Ethernet.cpp")
endif()

if((NOT CONFIG_OPENTHREAD_ENABLED) OR (NOT CONFIG_ENABLE_MATTER_OVER_THREAD))
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/OpenthreadLauncher.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/OpenthreadLauncher_LwIP.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/OpenthreadLauncher_NoLwIP.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/ThreadStackManagerImpl.cpp")
elseif(CONFIG_CHIP_USE_OT_ENDPOINT)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/OpenthreadLauncher_LwIP.cpp")
else()
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/OpenthreadLauncher_NoLwIP.cpp")
endif()

if(CONFIG_CHIP_USE_OT_ENDPOINT)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/DiagnosticDataProviderImpl_LwIP.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32Utils_LwIP.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/PlatformManagerImpl_LwIP.cpp")
else()
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/DiagnosticDataProviderImpl_OT.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32Utils_NoLwIP.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/PlatformManagerImpl_NoLwIP.cpp")
endif()

if(NOT CONFIG_ENABLE_OTA_REQUESTOR)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/OTAImageProcessorImpl.cpp")
endif()

if(NOT CONFIG_ENABLE_ESP32_FACTORY_DATA_PROVIDER)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32FactoryDataProvider.cpp")
endif()

if(NOT CONFIG_ENABLE_ESP32_DEVICE_INFO_PROVIDER)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32DeviceInfoProvider.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/StaticESP32DeviceInfoProvider.cpp")
endif()

if(NOT CONFIG_SEC_CERT_DAC_PROVIDER)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32SecureCertDataProvider.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32SecureCertDACProvider.cpp")
endif()

if(NOT CONFIG_USE_ESP32_ECDSA_PERIPHERAL)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/ESP32CHIPCryptoPAL.cpp")
endif()

if(CONFIG_CHIP_MEM_ALLOC_MODE_INTERNAL)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/CHIPMem-PlatformExternal.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/CHIPMem-PlatformDefault.cpp")
elseif(CONFIG_CHIP_MEM_ALLOC_MODE_EXTERNAL)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/CHIPMem-PlatformInternal.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/CHIPMem-PlatformDefault.cpp")
elseif(CONFIG_CHIP_MEM_ALLOC_MODE_DEFAULT)
    list(APPEND EXPLANT_EXCLUDE_SRCS_LIST
        "${MATTER_SDK_PATH}/src/platform/ESP32/CHIPMem-PlatformInternal.cpp"
        "${MATTER_SDK_PATH}/src/platform/ESP32/CHIPMem-PlatformExternal.cpp")
endif()
