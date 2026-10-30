"""Tests for nRF52 per-board bootloader selection and validation."""

import pytest

from esphome.components import nrf52
from esphome.components.nrf52 import boards
from esphome.components.nrf52.const import BOOTLOADER_ADAFRUIT
from esphome.components.zephyr import zephyr_data, zephyr_set_core_data
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_EXTRA_BUILD_FILES,
    KEY_OVERLAY,
    KEY_PM_STATIC,
    KEY_PRJ_CONF,
    KEY_SYSBUILD,
    KEY_SYSBUILD_CONF,
)
import esphome.config_validation as cv
from esphome.const import CONF_BOARD


def test_xiao_ble_accepts_mcuboot_bootloader() -> None:
    """xiao_ble lists mcuboot as a supported two-slot bootloader."""
    config = nrf52._detect_bootloader(
        {CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
    )

    assert config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT


def test_board_rejects_a_bootloader_it_does_not_list(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Every board in the real table now lists mcuboot, so use a stand-in.

    This covers the guard itself rather than a fact about any one board: a board
    that offers a restricted set must still refuse anything outside it.
    """
    monkeypatch.setitem(
        boards.BOARDS_ZEPHYR,
        "restricted_board",
        {KEY_BOOTLOADER: [BOOTLOADER_ADAFRUIT]},
    )

    with pytest.raises(cv.Invalid, match="does not support"):
        nrf52._detect_bootloader(
            {CONF_BOARD: "restricted_board", KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
        )


def test_xiao_ble_default_bootloader_is_not_mcuboot() -> None:
    """Without an explicit selection xiao_ble keeps its first (Adafruit) default."""
    config = nrf52._detect_bootloader({CONF_BOARD: "xiao_ble"})

    assert config[KEY_BOOTLOADER] != BOOTLOADER_MCUBOOT


@pytest.fixture(params=["xiao_ble", "adafruit_itsybitsy_nrf52840", "some_other_board"])
def xiao_ble_mcuboot(request: pytest.FixtureRequest) -> None:
    """The MCUboot setup is board-agnostic, so assert it on more than one."""
    zephyr_set_core_data(
        {CONF_BOARD: request.param, KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
    )
    nrf52._mcuboot_to_code()


def test_every_adafruit_board_can_select_mcuboot() -> None:
    """The codeowner asked for any board, not just the one this started on."""
    adafruit_boards = [
        board
        for board, info in boards.BOARDS_ZEPHYR.items()
        if BOOTLOADER_ADAFRUIT in info[KEY_BOOTLOADER]
    ]
    assert adafruit_boards
    for board in adafruit_boards:
        config = nrf52._detect_bootloader(
            {CONF_BOARD: board, KEY_BOOTLOADER: BOOTLOADER_MCUBOOT}
        )
        assert config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT
        # ...without becoming any board's default, which would silently change
        # what an existing config builds.
        assert nrf52._detect_bootloader({CONF_BOARD: board})[KEY_BOOTLOADER] != (
            BOOTLOADER_MCUBOOT
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

    assert nrf52._mcuboot_replaces_factory_bootloader(config) is expected


def test_mcuboot_image_is_built(xiao_ble_mcuboot: None) -> None:
    """The app links into a slot and sysbuild builds the bootloader alongside it."""
    app_conf = zephyr_data()[KEY_PRJ_CONF][""]

    assert zephyr_data()[KEY_SYSBUILD] is True
    assert app_conf["CONFIG_BOOTLOADER_MCUBOOT"][0] is True
    # No Adafruit bootloader is left to consume a UF2, and the one that would be
    # emitted carries the wrong load address.
    assert app_conf["CONFIG_BUILD_OUTPUT_UF2"][0] is False


def test_upgrade_mode_and_signing_are_set_at_sysbuild_level(
    xiao_ble_mcuboot: None,
) -> None:
    """Set anywhere else, sysbuild's FORCED_CONF_FILE silently overrides them."""
    assert zephyr_data()[KEY_SYSBUILD_CONF] == {
        "SB_CONFIG_BOOT_SIGNATURE_TYPE_NONE": True,
        "SB_CONFIG_MCUBOOT_MODE_OVERWRITE_ONLY": True,
    }


def test_flash_map_leaves_one_gap_for_the_primary_slot(xiao_ble_mcuboot: None) -> None:
    """The Partition Manager fills the single hole; everything else is pinned."""
    sections = {s.name: s for s in zephyr_data()[KEY_PM_STATIC]}

    assert set(sections) == {"mcuboot", "mcuboot_secondary", "settings_storage"}
    # The gap between the bootloader and the secondary slot is the primary slot,
    # and it has to be the same size as the secondary or an image that fits one
    # will not fit the other.
    primary_start = sections["mcuboot"].end_address
    primary_size = sections["mcuboot_secondary"].address - primary_start
    assert primary_start == nrf52.MCUBOOT_PARTITION_SIZE
    assert primary_size == sections["mcuboot_secondary"].size
    assert primary_size % nrf52.FLASH_SECTOR_SIZE == 0
    # Settings run to the end of the part, and PM would otherwise squeeze them
    # down to the two sectors NVS needs.
    assert sections["settings_storage"].end_address == nrf52.NRF52840_FLASH_SIZE
    assert sections["settings_storage"].size == nrf52.SETTINGS_PARTITION_SIZE
    # Nothing overlaps and nothing is stranded past the end of flash.
    assert primary_start + 2 * primary_size == sections["settings_storage"].address


def test_slot_labels_reach_both_images(xiao_ble_mcuboot: None) -> None:
    """MCUboot sizes its sectors from them; image signing sizes the app from them."""
    overlays = zephyr_data()[KEY_OVERLAY]
    sections = {s.name: s for s in zephyr_data()[KEY_PM_STATIC]}

    for image in ("", "mcuboot"):
        # The labels must agree with the pm_static map or MCUboot computes its
        # sector maths against a layout the device does not have.
        assert f"partition@{nrf52.MCUBOOT_PARTITION_SIZE:x}" in overlays[image]
        assert f"partition@{sections['mcuboot_secondary'].address:x}" in overlays[image]
    # Serial recovery over USB CDC only reaches the bootloader, and it binds to
    # a CDC node the overlay declares rather than a board-specific label.
    assert "zephyr,uart-mcumgr = &mcuboot_cdc_acm" in overlays["mcuboot"]
    assert "zephyr,uart-mcumgr" not in overlays[""]


def test_bootloader_kconfig_fragment_is_delivered(xiao_ble_mcuboot: None) -> None:
    """The fragment reaches the MCUboot image's prj_conf, which zephyr writes to
    ${APP_DIR}/sysbuild/mcuboot.conf together with other components' options."""
    mcuboot_conf = zephyr_data()[KEY_PRJ_CONF]["mcuboot"]

    assert mcuboot_conf["CONFIG_MCUBOOT_SERIAL"][0] is True
    assert mcuboot_conf["CONFIG_BUILD_OUTPUT_UF2"][0] is False
    assert mcuboot_conf["CONFIG_BOOT_SERIAL_WAIT_FOR_DFU_TIMEOUT"][0] == 30000
    assert mcuboot_conf["CONFIG_BOOT_SIGNATURE_KEY_FILE"][0] == "root-ec-p256.pem"
    assert "zephyr/sysbuild/mcuboot.conf" not in zephyr_data()[KEY_EXTRA_BUILD_FILES]
