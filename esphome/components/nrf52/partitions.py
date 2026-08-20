"""Shared flash-partition overlay math for nRF52 zigbee (ZBOSS) and OTA (mcumgr) support.

NCS 3.4.0 removed the implicit Partition Manager behavior that used to supply
storage/settings/mcuboot partitions regardless of what the raw board devicetree said, and
ncs-zigbee (R23) hard-requires real `zboss_nvram`/`zboss_product_config` fixed-partitions to
exist. Both needs land on the same physical flash, so they're computed together here instead
of by two independent, potentially-overlapping overlays.

Two devicetree schemes are in play for the nrf52840 boards this applies to (adafruit_itsybitsy,
adafruit_feather_nrf52840, xiao_ble) -- selected by the *board devicetree*, not directly by the
`bootloader:` config value, though in this repo's supported boards the two happen to coincide:
  - "native" scheme: the board's own stock devicetree already defines a full MCUboot dual-slot
    layout (`boot_partition`/`slot0_partition`/`slot1_partition`/`storage_partition`), used when
    `bootloader: mcuboot` (e.g. the bare `adafruit_feather_nrf52840` board string).
  - "UF2" scheme: the board's stock devicetree defines a single `code_partition` sized for the
    Adafruit UF2 bootloader + SoftDevice combo, used for the Adafruit-family bootloaders
    (`adafruit_itsybitsy/nrf52840` always uses this; `xiao_ble` always uses this; a
    `adafruit_feather_nrf52840` board string with an explicit `/nrf52840/uf2` qualifier would
    too). `.boards.BOOTLOADER_CONFIG` gives the SoftDevice/bootloader
    section addresses this scheme is built from.
"""

from esphome.components.zephyr import Section

# 36K total, split into a 32K NVRAM area and a 4K product-config area -- sizes fixed by
# ncs-zigbee itself (modules/ncs-zigbee/dts/nrf52840_partitions.dtsi in the zigbee SDK cache).
ZBOSS_NVRAM_SIZE = 0x8000
ZBOSS_PRODUCT_CONFIG_SIZE = 0x1000
ZBOSS_TOTAL_SIZE = ZBOSS_NVRAM_SIZE + ZBOSS_PRODUCT_CONFIG_SIZE

# Matches the stock UF2-scheme storage_partition size (nrf52840_partition_uf2_sdv6/7.dtsi).
STORAGE_SIZE = 0x8000
# Gap Adafruit's own UF2 bootloader leaves between the SoftDevice and the app for its own use.
MCUBOOT_SIZE = 0x9000

# Native scheme addresses (zephyr/dts/vendor/nordic/nrf52840_partition.dtsi) -- fixed, not
# board-parametrized, since it's Zephyr's own generic nrf52840 layout.
_NATIVE_SLOT1_START = 0x82000
_NATIVE_SLOT1_END = 0xF8000


def _partition(name: str, start: int, size: int, mapped: bool = False) -> str:
    compatible = (
        '\n                    compatible = "zephyr,mapped-partition";'
        if mapped
        else ""
    )
    return f"""
                {name}: partition@{start:x} {{{compatible}
                    reg = <0x{start:x} 0x{size:x}>;
                }};"""


def _zboss_partitions(start: int, mapped: bool = False) -> str:
    return _partition("zboss_nvram", start, ZBOSS_NVRAM_SIZE, mapped) + _partition(
        "zboss_product_config",
        start + ZBOSS_NVRAM_SIZE,
        ZBOSS_PRODUCT_CONFIG_SIZE,
        mapped,
    )


def native_scheme_zigbee_overlay() -> str:
    """Shrink the native scheme's slot1_partition tail to make room for zboss partitions.

    ota/zephyr_mcumgr needs nothing extra here -- the native scheme already has everything
    it needs regardless of zigbee.
    """
    zboss_start = _NATIVE_SLOT1_END - ZBOSS_TOTAL_SIZE
    new_slot1_size = zboss_start - _NATIVE_SLOT1_START
    return f"""
        &slot1_partition {{
            reg = <0x{_NATIVE_SLOT1_START:x} 0x{new_slot1_size:x}>;
        }};

        &flash0 {{
            partitions {{{_zboss_partitions(zboss_start, mapped=True)}
            }};
        }};
    """


def _sd_end(sections: list[Section]) -> int:
    return next(s.address + s.size for s in sections if "SoftDevice" in s.name)


def _bl_start(sections: list[Section]) -> int:
    return next(s.address for s in sections if "Adafruit" in s.name)


def uf2_scheme_code_partition_overlay(sections: list[Section]) -> str:
    """zigbee, UF2 scheme, no ota: shrink code_partition's tail for zboss partitions.

    No mcuboot dual-slot rebuild needed -- ota isn't in play, so code_partition stays.
    """
    sd_end = _sd_end(sections)
    storage_start = (
        _bl_start(sections) - STORAGE_SIZE
    )  # matches stock storage_partition start
    zboss_start = storage_start - ZBOSS_TOTAL_SIZE
    code_partition_size = zboss_start - sd_end
    return f"""
        &code_partition {{
            reg = <0x{sd_end:x} 0x{code_partition_size:x}>;
        }};

        &flash0 {{
            partitions {{{_zboss_partitions(zboss_start)}
            }};
        }};
    """


def uf2_scheme_rebuild_overlay(sections: list[Section], zigbee: bool) -> str:
    """OTA (UF2 scheme, non-mcuboot bootloader): rebuild boot/slot0/slot1/storage from scratch.

    Previously Partition Manager auto-supplied storage/boot partitions regardless of what this
    raw devicetree said; NCS 3.4.0 no longer does that, so they're carved out explicitly. When
    zigbee is also present, the zboss partitions are reserved from the same rebuild so the two
    never overlap (they used to, when computed by two independent overlays).
    """
    sd_end = _sd_end(sections)
    bl_start = _bl_start(sections)
    slot0_start = sd_end + MCUBOOT_SIZE
    reserved = STORAGE_SIZE + (ZBOSS_TOTAL_SIZE if zigbee else 0)
    # Align slot size down to a 4 KB sector boundary
    slot_size = ((bl_start - slot0_start - reserved) // 2 // 0x1000) * 0x1000
    slot1_start = slot0_start + slot_size
    storage_start = slot1_start + slot_size

    extra = ""
    if zigbee:
        extra = _zboss_partitions(storage_start + STORAGE_SIZE)

    return f"""
        /delete-node/ &boot_partition;
        /delete-node/ &storage_partition;
        /delete-node/ &code_partition;
        /delete-node/ &reserved_partition_0;

        &flash0 {{
            partitions {{
                compatible = "fixed-partitions";
                #address-cells = <1>;
                #size-cells = <1>;
                {_partition("boot_partition", sd_end, MCUBOOT_SIZE)}
                {_partition("slot0_partition", slot0_start, slot_size)}
                {_partition("slot1_partition", slot1_start, slot_size)}
                {_partition("storage_partition", storage_start, STORAGE_SIZE)}{extra}
            }};
        }};
    """
