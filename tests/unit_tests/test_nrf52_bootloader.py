"""Tests for nRF52 per-board bootloader selection and the MCUboot flash map."""

import pytest

from esphome.components import nrf52, zephyr_coredump
from esphome.components.nrf52 import boards
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT, BOOTLOADER_NRF
from esphome.components.zephyr import zephyr_add_pm_static, zephyr_data
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_SYSBUILD,
    KEY_SYSBUILD_CONF,
)
import esphome.config_validation as cv
from esphome.const import CONF_BOARD, KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE

FLASH_MAP_BOARDS = [
    board
    for board, info in boards.BOARDS_ZEPHYR.items()
    if info.get(boards.KEY_MCUBOOT_FLASH_MAP)
]


def _mcuboot_flash_map(coredump: bool = False) -> dict[str, nrf52.Section]:
    """Run the MCUboot flash map steps in code generation order."""
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version(2, 9, 2)}
    nrf52.set_core_data({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT})
    nrf52._mcuboot_to_code()
    if coredump:
        address = zephyr_coredump._partition_address()
        zephyr_add_pm_static(
            [
                nrf52.Section(
                    "coredump_partition",
                    address,
                    zephyr_coredump.PARTITION_SIZE,
                    "flash_primary",
                )
            ]
        )
    nrf52._mcuboot_slots()
    return {s.name: s for s in zephyr_data()[KEY_PM_STATIC]}


@pytest.fixture
def mcuboot_flash_map() -> dict[str, nrf52.Section]:
    return _mcuboot_flash_map()


def test_board_rejects_a_bootloader_it_does_not_list(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setitem(
        boards.BOARDS_ZEPHYR,
        "restricted_board",
        {KEY_BOOTLOADER: [BOOTLOADER_ADAFRUIT]},
    )

    with pytest.raises(cv.Invalid, match="does not support"):
        nrf52._detect_bootloader(
            {CONF_BOARD: "restricted_board", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
        )


@pytest.mark.parametrize("board", FLASH_MAP_BOARDS)
def test_flash_map_boards_offer_mcuboot_but_never_default(board: str) -> None:
    config = nrf52._detect_bootloader(
        {CONF_BOARD: board, KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
    )

    assert config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT
    assert nrf52._detect_bootloader({CONF_BOARD: board})[KEY_BOOTLOADER] != (
        BOOTLOADER_MCUBOOT
    )


def test_dongle_keeps_its_open_dfu_bootloader() -> None:
    with pytest.raises(cv.Invalid, match=f"select one of: {BOOTLOADER_NRF}$"):
        nrf52._detect_bootloader(
            {CONF_BOARD: "nrf52840dongle", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
        )


@pytest.mark.parametrize(
    ("board", "expected"),
    [
        ("xiao_ble", True),
        ("adafruit_itsybitsy_nrf52840", True),
        # No bootloader table: MCUboot is the default and the board's own
        # devicetree already carries the MCUboot partitions.
        ("adafruit_feather_nrf52840", False),
        ("nrf52840dk", False),
    ],
)
def test_flash_map_only_replaces_a_factory_bootloader(
    board: str, expected: bool
) -> None:
    config = nrf52._detect_bootloader(
        {CONF_BOARD: board, KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
    )

    assert nrf52._mcuboot_replaces_factory_bootloader(config) == expected


def test_mcuboot_image_is_built(mcuboot_flash_map: dict) -> None:
    """The app links into a slot and sysbuild builds the bootloader alongside it."""
    app_conf = zephyr_data()[KEY_PRJ_CONF][""]

    assert zephyr_data()[KEY_SYSBUILD] is True
    assert app_conf["CONFIG_BOOTLOADER_MCUBOOT"][0] is True
    assert app_conf["CONFIG_BUILD_OUTPUT_UF2"][0] is False


def test_upgrade_mode_and_signing_are_set_at_sysbuild_level(
    mcuboot_flash_map: dict,
) -> None:
    assert zephyr_data()[KEY_SYSBUILD_CONF] == {
        "SB_CONFIG_BOOT_SIGNATURE_TYPE_NONE": (True, True),
        "SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY": (True, True),
    }


def test_flash_map_leaves_one_gap_for_the_primary_slot(
    mcuboot_flash_map: dict,
) -> None:
    sections = mcuboot_flash_map

    assert set(sections) == {"mcuboot", "mcuboot_secondary", "settings_storage"}
    # The primary slot is the gap below the secondary and must be the same size
    primary_start = sections["mcuboot"].end_address
    primary_size = sections["mcuboot_secondary"].address - primary_start
    assert primary_start == boards.MCUBOOT_PARTITION_SIZE
    assert primary_size == sections["mcuboot_secondary"].size
    assert primary_size % 0x1000 == 0
    assert sections["settings_storage"].end_address == boards.NRF52840_FLASH_SIZE
    assert sections["settings_storage"].size == boards.SETTINGS_PARTITION_SIZE
    assert (
        sections["mcuboot_secondary"].end_address
        == sections["settings_storage"].address
    )


def test_flash_map_leaves_room_for_coredump() -> None:
    """zephyr_coredump goes directly below settings_storage, above the slots."""
    sections = _mcuboot_flash_map(coredump=True)

    assert (
        sections["mcuboot_secondary"].end_address
        == sections["coredump_partition"].address
    )
    assert (
        sections["coredump_partition"].end_address
        == sections["settings_storage"].address
    )


def test_flash_map_uses_the_testing_flash() -> None:
    """Grouped CI builds fake a larger flash; the map must span it to leave one gap."""
    CORE.testing_mode = True
    sections = _mcuboot_flash_map()

    assert sections["settings_storage"].end_address == nrf52.TESTING_FLASH_SIZE
    assert (
        sections["mcuboot_secondary"].end_address
        == sections["settings_storage"].address
    )


def test_slot_nodes_reach_both_images(mcuboot_flash_map: dict) -> None:
    """MCUboot sizes its sectors from them; image signing sizes the app from them."""
    overlays = zephyr_data()[KEY_OVERLAY]
    secondary = mcuboot_flash_map["mcuboot_secondary"]

    for image in ("", "mcuboot"):
        assert (
            f"slot0_partition: partition@{boards.MCUBOOT_PARTITION_SIZE:x}"
            in overlays[image]
        )
        assert f"slot1_partition: partition@{secondary.address:x}" in overlays[image]
    # Serial recovery binds to a CDC node the overlay declares, not a board label
    assert "zephyr,uart-mcumgr = &mcuboot_cdc_acm" in overlays["mcuboot"]
    assert "zephyr,uart-mcumgr" not in overlays[""]


def test_bootloader_kconfig_fragment_is_delivered(mcuboot_flash_map: dict) -> None:
    mcuboot_conf = zephyr_data()[KEY_PRJ_CONF]["mcuboot"]

    assert mcuboot_conf["CONFIG_MCUBOOT_SERIAL"][0] is True
    assert mcuboot_conf["CONFIG_BOOT_SERIAL_WAIT_FOR_DFU_TIMEOUT"][0] == 30000
    assert mcuboot_conf["CONFIG_BOOT_SIGNATURE_KEY_FILE"][0] == "root-ec-p256.pem"
