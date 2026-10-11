# ESPHome flash layout checks. Inlined into the generated sysbuild CMakeLists.txt and
# run after find_package(Sysbuild), when every image is configured: dt_* calls read the
# merged devicetree (board, shields, snippets, overlays) of the image named by TARGET,
# and SB_CONFIG_* hold what sysbuild actually builds.
#
# Addresses are a partition's raw `reg`: the offset within its flash device.

function(_esphome_fail)
  string(JOIN "" msg ${ARGN})
  message(FATAL_ERROR "ESPHome: ${msg} Fix the flash layout with 'zephyr: overlays:' "
    "(an MCUboot build needs the same layout in the app and mcuboot overlays).")
endfunction()

function(_esphome_hex out value)
  math(EXPR hex "${value}" OUTPUT_FORMAT HEXADECIMAL)
  set(${out} ${hex} PARENT_SCOPE)
endfunction()

# Sets <out>_path, <out>_start, <out>_end and <out>_dev (the flash device node) for the
# partition at `path` in `image`. <out>_path is empty when `path` is.
function(_esphome_node out image path)
  set(${out}_path "${path}" PARENT_SCOPE)
  if(NOT path)
    return()
  endif()
  dt_prop(reg TARGET ${image} PATH "${path}" PROPERTY "reg")
  list(LENGTH reg reg_len)
  if(reg_len LESS 2)
    _esphome_fail("Partition '${path}' in image '${image}' has no usable reg property.")
  endif()
  list(GET reg 0 start)
  list(GET reg 1 size)
  math(EXPR start "${start}")
  math(EXPR end "${start} + ${size}")
  string(REGEX REPLACE "/partitions(/.*)?$" "" dev "${path}")
  set(${out}_start ${start} PARENT_SCOPE)
  set(${out}_end ${end} PARENT_SCOPE)
  set(${out}_dev "${dev}" PARENT_SCOPE)
endfunction()

macro(_esphome_label out image label)
  dt_nodelabel(_esphome_path TARGET ${image} NODELABEL "${label}")
  _esphome_node(${out} ${image} "${_esphome_path}")
endmacro()

macro(_esphome_chosen out image property)
  dt_chosen(_esphome_path TARGET ${image} PROPERTY "${property}")
  _esphome_node(${out} ${image} "${_esphome_path}")
endmacro()

# BOOT_START: flash offset where the chip (or a resident vendor bootloader) starts the
#   first image; omitted = not checked.
# RESERVED: start/end pairs no written partition may overlap, on the first image's device.
# REQUIRE: label/reason pairs for partitions a component needs.
function(esphome_check_partitions)
  cmake_parse_arguments(ARG "" "BOOT_START" "RESERVED;REQUIRE" ${ARGN})
  set(app ${DEFAULT_IMAGE})
  set(written)

  # An image without a chosen code partition is linked at the start of its flash.
  _esphome_chosen(app_code ${app} "zephyr,code-partition")
  if(app_code_path)
    list(APPEND written app_code)
  else()
    dt_chosen(app_code_dev TARGET ${app} PROPERTY "zephyr,flash")
    set(app_code_start 0)
  endif()
  set(first app_code)
  set(first_what "The application")

  if(SB_CONFIG_BOOTLOADER_MCUBOOT)
    set(slots boot_partition slot0_partition)
    if(SB_CONFIG_MCUBOOT_MODE_SINGLE_APP)
      foreach(image ${app} mcuboot)
        dt_nodelabel(slot1 TARGET ${image} NODELABEL "slot1_partition")
        if(DEFINED slot1)
          _esphome_fail("'single_slot: true' needs a layout without slot1_partition, "
            "but image '${image}' still has one.")
        endif()
      endforeach()
    else()
      list(APPEND slots slot1_partition)
    endif()
    if(SB_CONFIG_MCUBOOT_MODE_SWAP_SCRATCH)
      list(APPEND slots scratch_partition)
    endif()
    foreach(label ${slots})
      _esphome_label(${label} ${app} ${label})
      _esphome_label(mcuboot_${label} mcuboot ${label})
      if(NOT ${label}_path OR NOT mcuboot_${label}_path)
        _esphome_fail("MCUboot needs a partition labeled '${label}' in both the app and "
          "mcuboot images.")
      endif()
      if(NOT ${label}_start EQUAL mcuboot_${label}_start
         OR NOT ${label}_end EQUAL mcuboot_${label}_end)
        _esphome_hex(a_start ${${label}_start})
        _esphome_hex(a_end ${${label}_end})
        _esphome_hex(m_start ${mcuboot_${label}_start})
        _esphome_hex(m_end ${mcuboot_${label}_end})
        _esphome_fail("The app and mcuboot images disagree on '${label}': "
          "${a_start}-${a_end} in the app, ${m_start}-${m_end} in mcuboot.")
      endif()
      list(APPEND written ${label})
    endforeach()

    if(NOT "${app_code_path}" STREQUAL "${slot0_partition_path}")
      _esphome_fail("With MCUboot the application must run from slot0_partition, but its "
        "zephyr,code-partition is '${app_code_path}'.")
    endif()
    dt_chosen(mcuboot_code TARGET mcuboot PROPERTY "zephyr,code-partition")
    if(NOT "${mcuboot_code}" STREQUAL "${mcuboot_boot_partition_path}")
      _esphome_fail("MCUboot must run from boot_partition, but the mcuboot image's "
        "zephyr,code-partition is '${mcuboot_code}'.")
    endif()
    if("${app}_slot1_variant" IN_LIST IMAGES)
      dt_chosen(variant_code TARGET ${app}_slot1_variant PROPERTY "zephyr,code-partition")
      dt_nodelabel(variant_slot1 TARGET ${app}_slot1_variant NODELABEL "slot1_partition")
      if(NOT variant_code OR NOT "${variant_code}" STREQUAL "${variant_slot1}")
        _esphome_fail("Direct-xip builds the second app image for slot1_partition, but its "
          "zephyr,code-partition is '${variant_code}'.")
      endif()
    endif()
    set(first boot_partition)
    set(first_what "MCUboot")
  endif()

  sysbuild_get(settings_nvs IMAGE ${app} VAR CONFIG_SETTINGS_NVS KCONFIG)
  sysbuild_get(settings_zms IMAGE ${app} VAR CONFIG_SETTINGS_ZMS KCONFIG)
  if(settings_nvs OR settings_zms)
    # Zephyr's settings backends use this chosen node when present (settings_nvs.c).
    _esphome_chosen(settings ${app} "zephyr,settings-partition")
    if(NOT settings_path)
      _esphome_label(settings ${app} storage_partition)
    endif()
    if(NOT settings_path)
      _esphome_fail("Preferences need a flash partition labeled 'storage_partition' (or a "
        "zephyr,settings-partition chosen node), and this build has none.")
    endif()
    if(settings_nvs)
      dt_prop(erase_block TARGET ${app} PATH "${settings_dev}" PROPERTY "erase-block-size")
      sysbuild_get(sector_mult IMAGE ${app} VAR CONFIG_SETTINGS_NVS_SECTOR_SIZE_MULT KCONFIG)
      if(erase_block AND sector_mult)
        # NVS needs at least two sectors.
        math(EXPR min_size "2 * ${erase_block} * ${sector_mult}")
        math(EXPR size "${settings_end} - ${settings_start}")
        if(size LESS min_size)
          _esphome_fail("The preferences partition '${settings_path}' is ${size} bytes; "
            "NVS needs at least ${min_size} (two sectors).")
        endif()
      endif()
    endif()
    list(APPEND written settings)
  endif()

  set(required ${ARG_REQUIRE})
  set(index 0)
  while(required)
    list(POP_FRONT required label reason)
    _esphome_label(required_${index} ${app} ${label})
    if(NOT required_${index}_path)
      _esphome_fail("${reason} needs a flash partition labeled '${label}', and this build "
        "has none.")
    endif()
    list(APPEND written required_${index})
    math(EXPR index "${index} + 1")
  endwhile()

  if(DEFINED ARG_BOOT_START)
    math(EXPR boot_start "${ARG_BOOT_START}")
    if(NOT ${first}_start EQUAL boot_start)
      _esphome_hex(want ${boot_start})
      _esphome_hex(got ${${first}_start})
      _esphome_fail("${first_what} must start at ${want}, where this chip or board starts "
        "running code, but it starts at ${got} ('${${first}_path}').")
    endif()
  endif()

  # Overlaps, only between partitions on the same flash device. One node can serve
  # two roles (e.g. the app's code partition is slot0_partition).
  set(rest ${written})
  while(rest)
    list(POP_FRONT rest a)
    foreach(b ${rest})
      if("${${a}_path}" STREQUAL "${${b}_path}"
         OR NOT "${${a}_dev}" STREQUAL "${${b}_dev}")
        continue()
      endif()
      if(${a}_start LESS ${b}_end AND ${b}_start LESS ${a}_end)
        _esphome_fail("Partitions '${${a}_path}' and '${${b}_path}' overlap.")
      endif()
    endforeach()
  endwhile()

  set(reserved ${ARG_RESERVED})
  while(reserved)
    list(POP_FRONT reserved r_start r_end)
    math(EXPR r_start "${r_start}")
    math(EXPR r_end "${r_end}")
    foreach(a ${written})
      if("${${a}_dev}" STREQUAL "${${first}_dev}"
         AND ${a}_start LESS r_end AND r_start LESS ${a}_end)
        _esphome_hex(hex_start ${r_start})
        _esphome_hex(hex_end ${r_end})
        _esphome_fail("Partition '${${a}_path}' overlaps ${hex_start}-${hex_end}, which "
          "this chip or board keeps for its own bootloader or firmware.")
      endif()
    endforeach()
  endwhile()
endfunction()
