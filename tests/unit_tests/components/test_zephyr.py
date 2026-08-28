"""Unit tests for esphome.components.zephyr (core helpers)."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.zephyr import (
    _MODULE_SCHEMA,
    _resolve_board_source,
    _resolve_shield_source,
    _resolve_snippet_source,
    _variant_config_schema,
    add_extra_build_file,
    add_extra_script,
    request_zephyr_module,
    resolve_zephyr_modules,
    upload_program,
    zephyr_add_kconfig,
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_add_sysbuild_conf,
    zephyr_add_user,
    zephyr_dts_board_id,
    zephyr_only_on_variant,
    zephyr_set_module_override,
    zephyr_setup_i2c_pinctrl,
    zephyr_setup_spi_pinctrl,
    zephyr_variant,
    zephyr_variant_family,
)
from esphome.components.zephyr.const import CONF_BOARD_SOURCE, KEY_ZEPHYR
from esphome.components.zephyr.variants import (
    VARIANTS,
    ZephyrModule,
    ZephyrModuleTemplate,
    ZephyrSDK,
    ZephyrVariant,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_FRAMEWORK,
    CONF_NAME,
    CONF_PATH,
    CONF_REFRESH,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_URL,
    CONF_VARIANT,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ZEPHYR,
    TYPE_GIT,
    TYPE_LOCAL,
)
from esphome.core import CORE, EsphomeError


def _set_non_nrf52_target_platform() -> None:
    # zephyr_setup_i2c_pinctrl() checks CORE.is_nrf52, which reads
    # CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] -- unset in a bare test, unlike a
    # real compile run where core config validation always populates it first.
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}


def _empty_zephyr_data(
    variant: str | None = None, framework_type: str = "zephyr"
) -> dict:
    variant_info = VARIANTS.get(variant) if variant is not None else None
    return {
        "board": "some_board",
        "board_root": None,
        "sdk_source": None,
        "bootloader": "none",
        "variant": variant,
        "family": variant_info.family if variant_info is not None else None,
        "framework_type": framework_type,
        "west_version": None,
        "ninja_version": None,
        "prj_conf": {},
        "sysbuild_conf": {},
        "overlay": {"": ""},
        "overlay_builder": [],
        "extra_build_files": {},
        "pm_static": [],
        "user": {},
        "kconfig": "",
        "fake_board_manifest": None,
        "dts_base_path": None,
        "i2c_bus_cache": {},
        "cpp_path": "",
        "board_dir_cache": {},
        "dts_include_paths": None,
        "board_edt_cache": {},
        "board_yaml_cache": {},
        "snippets": [],
        "module_requests": {},
        "module_overrides": {},
    }


# ---------------------------------------------------------------------------
# zephyr_variant / zephyr_variant_family
# ---------------------------------------------------------------------------


def test_zephyr_variant_returns_variant_name() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32H2")
    assert zephyr_variant() == "ESP32H2"


def test_zephyr_variant_returns_none_when_unset() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant=None)
    assert zephyr_variant() is None


def test_zephyr_variant_family_returns_none_when_variant_unset() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant=None)
    assert zephyr_variant_family() is None


def test_zephyr_variant_family_returns_none_for_unregistered_variant() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NOT_A_REAL_VARIANT")
    assert zephyr_variant_family() is None


def test_zephyr_variant_family_returns_family_for_esp32h2() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32H2")
    assert zephyr_variant_family() == "esp32"


def test_zephyr_variant_family_returns_family_for_esp32c6() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32C6")
    assert zephyr_variant_family() == "esp32"


def test_zephyr_variant_family_returns_none_for_native_sim() -> None:
    # native_sim has no silicon family -- it's not a real chip.
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NATIVESIM")
    assert zephyr_variant_family() is None


# ---------------------------------------------------------------------------
# zephyr_only_on_variant
# ---------------------------------------------------------------------------


def test_zephyr_only_on_variant_passes_matching_variant() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32H2")
    validator = zephyr_only_on_variant("ESP32H2", "ESP32C6")
    assert validator("value") == "value"


def test_zephyr_only_on_variant_raises_for_non_matching_variant() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NATIVESIM")
    validator = zephyr_only_on_variant("ESP32H2", "ESP32C6")
    with pytest.raises(cv.Invalid):
        validator("value")


# ---------------------------------------------------------------------------
# zephyr_add_prj_conf -- conflicting required value detection
# ---------------------------------------------------------------------------


def test_zephyr_add_prj_conf_first_set_records_value() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_prj_conf("FOO", True)
    assert CORE.data[KEY_ZEPHYR]["prj_conf"][""]["CONFIG_FOO"] == (True, True)


def test_zephyr_add_prj_conf_prefixes_config_automatically() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_prj_conf("CONFIG_ALREADY_PREFIXED", True)
    assert "CONFIG_CONFIG_ALREADY_PREFIXED" not in CORE.data[KEY_ZEPHYR]["prj_conf"][""]
    assert "CONFIG_ALREADY_PREFIXED" in CORE.data[KEY_ZEPHYR]["prj_conf"][""]


def test_zephyr_add_prj_conf_same_value_twice_does_not_raise() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_prj_conf("FOO", True)
    zephyr_add_prj_conf("FOO", True)  # no-op, same value


def test_zephyr_add_prj_conf_raises_on_conflicting_required_value() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_prj_conf("FOO", True, required=True)
    with pytest.raises(ValueError, match="already set"):
        zephyr_add_prj_conf("FOO", False, required=True)


def test_zephyr_add_prj_conf_differing_value_after_required_always_raises() -> None:
    # The *first* call's required flag is what locks the value -- a later call
    # for a different value raises regardless of whether that later call itself
    # asks for required=True or required=False.
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_prj_conf("FOO", True, required=True)
    with pytest.raises(ValueError, match="already set"):
        zephyr_add_prj_conf("FOO", False, required=False)


def test_zephyr_add_prj_conf_non_required_initial_value_silently_ignored() -> None:
    # When the *first* call was required=False, a later differing value doesn't
    # raise -- but it's only actually written if that later call is itself
    # required=True; a later required=False call is silently dropped.
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_prj_conf("FOO", True, required=False)
    zephyr_add_prj_conf("FOO", False, required=False)
    assert CORE.data[KEY_ZEPHYR]["prj_conf"][""]["CONFIG_FOO"] == (True, False)


def test_zephyr_add_prj_conf_required_call_after_non_required_wins() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_prj_conf("FOO", True, required=False)
    zephyr_add_prj_conf("FOO", False, required=True)
    assert CORE.data[KEY_ZEPHYR]["prj_conf"][""]["CONFIG_FOO"] == (False, True)


# ---------------------------------------------------------------------------
# zephyr_add_overlay
# ---------------------------------------------------------------------------


def test_zephyr_add_overlay_dedents_and_appends() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_overlay("""
        &foo { status = "okay"; };
    """)
    assert '&foo { status = "okay"; };' in CORE.data[KEY_ZEPHYR]["overlay"][""]


def test_zephyr_add_overlay_appends_across_multiple_calls() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_overlay('&a { status = "okay"; };')
    zephyr_add_overlay('&b { status = "okay"; };')
    overlay = CORE.data[KEY_ZEPHYR]["overlay"][""]
    assert "&a" in overlay
    assert "&b" in overlay


# ---------------------------------------------------------------------------
# zephyr_setup_i2c_pinctrl -- the mechanism ZEPHYR_ESP32_ADC1_PIN_TO_CHANNEL's
# sibling feature (get_i2c_pinctrl_esp32) plugs into via pinctrl_extractors.
# ---------------------------------------------------------------------------


def test_zephyr_setup_i2c_pinctrl_returns_explicit_pins_unchanged() -> None:
    _set_non_nrf52_target_platform()
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32H2")
    sda, scl = zephyr_setup_i2c_pinctrl("some_board", "i2c0", sda=5, scl=6)
    assert (sda, scl) == (5, 6)


def test_zephyr_setup_i2c_pinctrl_uses_extractor_when_pins_not_given(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _set_non_nrf52_target_platform()
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="FAKE_VARIANT")

    def fake_extractor(board: str, bus_label: str) -> dict[str, int]:
        assert board == "some_board"
        assert bus_label == "i2c0"
        return {"sda": 6, "scl": 7}

    fake_variant = ZephyrVariant(
        sdk=ZephyrSDK(manifest_url="http://dummy"),
        pinctrl_extractors={"i2c": fake_extractor},
    )
    monkeypatch.setitem(VARIANTS, "FAKE_VARIANT", fake_variant)

    sda, scl = zephyr_setup_i2c_pinctrl("some_board", "i2c0", sda=None, scl=None)
    assert (sda, scl) == (6, 7)


def test_zephyr_setup_i2c_pinctrl_raises_when_extractor_returns_none(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="FAKE_VARIANT")

    fake_variant = ZephyrVariant(
        sdk=ZephyrSDK(manifest_url="http://dummy"),
        pinctrl_extractors={"i2c": lambda board, bus_label: None},
    )
    monkeypatch.setitem(VARIANTS, "FAKE_VARIANT", fake_variant)

    with pytest.raises(cv.Invalid, match="Could not determine I2C pin assignments"):
        zephyr_setup_i2c_pinctrl("some_board", "i2c0", sda=None, scl=None)


def test_zephyr_setup_i2c_pinctrl_raises_when_no_extractor_and_no_pins() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NATIVESIM")
    # native_sim's own entry has no "i2c" pinctrl_extractor and none were given.
    with pytest.raises(cv.Invalid, match="Could not determine I2C pin assignments"):
        zephyr_setup_i2c_pinctrl("native_sim/native/64", "i2c0", sda=None, scl=None)


def test_zephyr_setup_i2c_pinctrl_native_sim_adds_status_overlay_only() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NATIVESIM")
    zephyr_setup_i2c_pinctrl("native_sim/native/64", "i2c0", sda=1, scl=2)
    overlay = CORE.data[KEY_ZEPHYR]["overlay"][""]
    assert '&i2c0 { status = "okay"; };' in overlay
    # No pinctrl node -- native_sim's emulated controller has no physical pins.
    assert "pinctrl" not in overlay


# ---------------------------------------------------------------------------
# zephyr_setup_spi_pinctrl
# ---------------------------------------------------------------------------


def test_zephyr_setup_spi_pinctrl_fixed_family_only_enables_bus() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="STM32L4")
    zephyr_setup_spi_pinctrl("some_board", "spi1")
    overlay = CORE.data[KEY_ZEPHYR]["overlay"][""]
    assert '&spi1 { status = "okay"; };' in overlay
    assert "pinctrl" not in overlay


def test_zephyr_setup_spi_pinctrl_raises_for_unimplemented_family() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52")
    with pytest.raises(cv.Invalid, match="not implemented yet"):
        zephyr_setup_spi_pinctrl("some_board", "spi0")


def test_zephyr_setup_spi_pinctrl_esp32_raises_without_clk() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32")
    with pytest.raises(cv.Invalid, match="Could not determine SPI pin assignments"):
        zephyr_setup_spi_pinctrl("some_board", "spi2")


def test_zephyr_setup_spi_pinctrl_esp32_single_bit_uses_hspi_prefix() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32")
    zephyr_setup_spi_pinctrl("some_board", "spi2", clk=6, miso=8, mosi=7)
    overlay = CORE.data[KEY_ZEPHYR]["overlay"][""]
    assert "SPIM2_SCLK_GPIO6" in overlay
    assert "SPIM2_MISO_GPIO8" in overlay
    assert "SPIM2_MOSI_GPIO7" in overlay
    assert "group2" not in overlay
    assert "&spi2 {" in overlay
    assert "pinctrl-0 = <&spi2_default>;" in overlay
    assert "CONFIG_SPI_EXTENDED_MODES" not in CORE.data[KEY_ZEPHYR]["prj_conf"].get(
        "", {}
    )


def test_zephyr_setup_spi_pinctrl_esp32_spi3_uses_vspi_prefix() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32")
    zephyr_setup_spi_pinctrl("some_board", "spi3", clk=36, miso=37, mosi=35)
    overlay = CORE.data[KEY_ZEPHYR]["overlay"][""]
    assert "SPIM3_SCLK_GPIO36" in overlay


def test_zephyr_setup_spi_pinctrl_esp32_c6_uses_fspi_prefix() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32C6")
    zephyr_setup_spi_pinctrl("some_board", "spi2", clk=6, miso=8, mosi=7)
    overlay = CORE.data[KEY_ZEPHYR]["overlay"][""]
    assert "SPIM2_SCLK_GPIO6" in overlay


def test_zephyr_setup_spi_pinctrl_esp32_quad_adds_wp_hd_group_and_extended_modes() -> (
    None
):
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32C6")
    zephyr_setup_spi_pinctrl("some_board", "spi2", clk=6, data_pins=[7, 8, 9, 10])
    overlay = CORE.data[KEY_ZEPHYR]["overlay"][""]
    # data_pins[0]/[1] (D0/D1) take the mosi/miso role since no separate mosi/miso
    # was passed for quad mode.
    assert "SPIM2_MOSI_GPIO7" in overlay
    assert "SPIM2_MISO_GPIO8" in overlay
    assert "group2" in overlay
    assert "ESP32_PINMUX(9, ESP_NOSIG, ESP_FSPIWP_OUT)" in overlay
    assert "ESP32_PINMUX(10, ESP_NOSIG, ESP_FSPIHD_OUT)" in overlay
    assert CORE.data[KEY_ZEPHYR]["prj_conf"][""]["CONFIG_SPI_EXTENDED_MODES"] == (
        True,
        True,
    )


# ---------------------------------------------------------------------------
# zephyr_dts_board_id
# ---------------------------------------------------------------------------


def test_zephyr_dts_board_id_returns_input_unchanged_for_non_nrf52() -> None:
    _set_non_nrf52_target_platform()
    assert zephyr_dts_board_id("esp32h2_devkitm/esp32h2") == "esp32h2_devkitm/esp32h2"


def test_zephyr_dts_board_id_falls_back_to_input_when_no_west_board_override() -> None:
    # No BOARDS_ZEPHYR entry currently sets "west_board" (the HWMv2 translation
    # this function documents is dormant, not yet needed by any real board) --
    # confirms the .get(..., esphome_board) fallback path, not a hardcoded value.
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "nrf52"}
    assert (
        zephyr_dts_board_id("adafruit_itsybitsy_nrf52840")
        == "adafruit_itsybitsy_nrf52840"
    )


def test_zephyr_dts_board_id_uses_west_board_override_when_present(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    from esphome.components.nrf52.boards import BOARDS_ZEPHYR

    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "nrf52"}
    monkeypatch.setitem(
        BOARDS_ZEPHYR, "some_board", {"west_board": "some_board/nrf52840"}
    )
    assert zephyr_dts_board_id("some_board") == "some_board/nrf52840"


# ---------------------------------------------------------------------------
# zephyr_add_sysbuild_conf -- mirrors zephyr_add_prj_conf's conflict semantics
# ---------------------------------------------------------------------------


def test_zephyr_add_sysbuild_conf_prefixes_sb_config_automatically() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    assert CORE.data[KEY_ZEPHYR]["sysbuild_conf"]["SB_CONFIG_BOOTLOADER_MCUBOOT"] == (
        True,
        True,
    )


def test_zephyr_add_sysbuild_conf_raises_on_conflicting_required_value() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_sysbuild_conf("FOO", True, required=True)
    with pytest.raises(ValueError, match="already set"):
        zephyr_add_sysbuild_conf("FOO", False, required=True)


# ---------------------------------------------------------------------------
# add_extra_build_file / add_extra_script
# ---------------------------------------------------------------------------


def test_add_extra_build_file_returns_true_on_first_add() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    from pathlib import Path

    assert add_extra_build_file("foo.h", Path("/tmp/foo.h")) is True
    assert CORE.data[KEY_ZEPHYR]["extra_build_files"]["foo.h"] == Path("/tmp/foo.h")


def test_add_extra_build_file_returns_false_when_already_present() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    from pathlib import Path

    add_extra_build_file("foo.h", Path("/tmp/foo.h"))
    assert add_extra_build_file("foo.h", Path("/tmp/other.h")) is False
    # First path wins, not silently overwritten.
    assert CORE.data[KEY_ZEPHYR]["extra_build_files"]["foo.h"] == Path("/tmp/foo.h")


def test_add_extra_script_only_registers_platformio_option_on_first_add() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    from pathlib import Path

    with patch("esphome.codegen.add_platformio_option") as mock_add_option:
        add_extra_script("pre", "foo.py", Path("/tmp/foo.py"))
        add_extra_script("pre", "foo.py", Path("/tmp/foo.py"))
    mock_add_option.assert_called_once_with("extra_scripts", ["pre:foo.py"])


# ---------------------------------------------------------------------------
# zephyr_add_kconfig / zephyr_add_pm_static / zephyr_add_user
# ---------------------------------------------------------------------------


def test_zephyr_add_kconfig_dedents_and_appends_newline() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_kconfig("""
        config FOO
        \tbool "foo"
    """)
    assert CORE.data[KEY_ZEPHYR]["kconfig"].endswith('\tbool "foo"\n\n')


def test_zephyr_add_pm_static_extends_sections_list() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_pm_static(["section_a"])
    zephyr_add_pm_static(["section_b"])
    assert CORE.data[KEY_ZEPHYR]["pm_static"] == ["section_a", "section_b"]


def test_zephyr_add_user_accumulates_multiple_values_under_same_key() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()
    zephyr_add_user("io-channels", "<&adc 0>")
    zephyr_add_user("io-channels", "<&adc 1>")
    assert CORE.data[KEY_ZEPHYR]["user"]["io-channels"] == ["<&adc 0>", "<&adc 1>"]


# ---------------------------------------------------------------------------
# _variant_config_schema -- structural/dispatch validation
# ---------------------------------------------------------------------------


def _init_variant_schema_core() -> None:
    CORE.data[KEY_CORE] = {}
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data()


def test_variant_config_schema_skips_validation_when_owned_by_other_platform() -> None:
    # nrf52 sets target_platform itself before auto-loading zephyr as a dependency
    # -- _variant_config_schema must not try to validate nrf52's config shape.
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: "nrf52"}
    config = {"some_nrf52_only_key": 123}
    assert _variant_config_schema(config) == config


def test_variant_config_schema_rejects_mac_address_on_non_native_sim() -> None:
    _init_variant_schema_core()
    with pytest.raises(cv.Invalid, match="mac_address"):
        _variant_config_schema(
            {"variant": "ESP32H2", "mac_address": "AA:BB:CC:DD:EE:FF"}
        )


def test_variant_config_schema_rejects_watchdog_timeout_on_native_sim() -> None:
    _init_variant_schema_core()
    with pytest.raises(cv.Invalid, match="watchdog_timeout"):
        _variant_config_schema({"variant": "NATIVESIM", "watchdog_timeout": "5s"})


def test_variant_config_schema_defaults_watchdog_timeout_for_real_hardware() -> None:
    _init_variant_schema_core()
    config = _variant_config_schema({"variant": "ESP32H2"})
    assert config["watchdog_timeout"] == cv.TimePeriod(seconds=10)


def test_variant_config_schema_raises_for_unregistered_variant(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    # CONF_VARIANT's cv.one_of(*VARIANTS) is built at import time, so monkeypatching
    # VARIANTS alone can't get a fake key past it -- bypass that outer schema to
    # exercise the dead-code guard directly.
    import esphome.components.zephyr as zephyr_module

    _init_variant_schema_core()
    fake_variant = ZephyrVariant(sdk=ZephyrSDK(manifest_url="http://dummy"))
    monkeypatch.setitem(VARIANTS, "FAKE_VARIANT", fake_variant)
    monkeypatch.setattr(zephyr_module, "_ZEPHYR_SCHEMA", lambda config: config)
    with pytest.raises(cv.Invalid, match="no config schema registered"):
        _variant_config_schema({"variant": "FAKE_VARIANT"})


# ---------------------------------------------------------------------------
# _variant_config_schema -- round-trip regression for #42: `esphome
# upload`/`logs` can skip full validation via the validated-config cache and
# re-run CONFIG_SCHEMA on the already-validated, cached zephyr: block in a
# fresh process (see upload_program()) -- each variant's config_schema() must
# return a dict that validates cleanly when fed back through _ZEPHYR_SCHEMA a
# second time. Covers every variant fixed by #43 (removal of the dead
# `config[KEY_FRAMEWORK_VERSION] = version_str` write) plus NATIVESIM, fixed
# separately in #47 (cv.mac_address() made idempotent on an already-parsed
# MACAddress).
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    "variant",
    ["ESP32", "ESP32C6", "ESP32H2", "NRF52", "NRF54L15", "NRF54LM20A", "NATIVESIM"],
)
def test_variant_config_schema_round_trips_cleanly(variant: str) -> None:
    _init_variant_schema_core()
    validated = _variant_config_schema({CONF_VARIANT: variant})

    # Simulate upload_program()'s cache-repopulation path: re-run CONFIG_SCHEMA on
    # the already-validated config in a fresh CORE.data state, exactly as it would
    # be in a separate `esphome upload` process.
    _init_variant_schema_core()
    re_validated = _variant_config_schema(validated)

    assert re_validated[CONF_VARIANT] == variant
    assert "framework_version" not in re_validated


# ---------------------------------------------------------------------------
# upload_program -- regression coverage for the NameError bug found/fixed
# this session (VARIANTS[variant] referenced an undefined local "variant")
# ---------------------------------------------------------------------------


def test_upload_program_returns_false_for_non_esp32_family(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NATIVESIM")
    assert upload_program({}, object(), "some_host") is False


def test_upload_program_returns_false_for_non_serial_host() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32H2")
    with patch("esphome.upload_targets.get_port_type", return_value="OTA"):
        assert upload_program({}, object(), "192.168.1.5") is False


def test_upload_program_resolves_variant_and_flashes(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """Regression test: previously crashed with NameError on VARIANTS[variant]
    (undefined local) instead of VARIANTS[zephyr_variant()]."""
    from esphome.const import KEY_FRAMEWORK_VERSION
    from esphome.upload_targets import PortType

    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="ESP32H2")
    CORE.data[KEY_CORE] = {
        KEY_FRAMEWORK_VERSION: "4.4.1",
        KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR,
    }
    CORE.config_path = tmp_path / "test.yaml"
    CORE.build_path = tmp_path / "build"

    with (
        patch("esphome.upload_targets.get_port_type", return_value=PortType.SERIAL),
        patch("esphome.__main__.check_permissions"),
        patch(
            "esphome.components.zephyr.framework_west.check_and_install",
            return_value=("python_bin", "framework_path", {}),
        ) as mock_west_install,
        patch(
            "esphome.components.zephyr.build_zephyr.run_west_flash", return_value=True
        ) as mock_flash,
    ):
        result = upload_program(
            {PLATFORM_ZEPHYR: {CONF_FRAMEWORK: {CONF_REFRESH: "1d"}}},
            object(),
            "/dev/ttyACM0",
        )

    assert result is True
    # Confirms VARIANTS[zephyr_variant()] resolved to the real ESP32H2 SDK, not a
    # crash -- west_install was called with that variant's actual sdk object.
    mock_west_install.assert_called_once()
    assert mock_west_install.call_args[0][0] is VARIANTS["ESP32H2"].sdk
    mock_flash.assert_called_once()


# ---------------------------------------------------------------------------
# upload_program -- KEY_ZEPHYR repopulation on the validated-config cache fast
# path (`esphome upload`/`logs` can skip full config validation, so the
# CORE.data[KEY_ZEPHYR] side effect of _variant_config_schema() never ran)
# ---------------------------------------------------------------------------


def test_upload_program_repopulates_zephyr_data_when_missing_from_cache() -> None:
    CORE.data.pop(KEY_ZEPHYR, None)
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    zephyr_config = {"variant": "NATIVESIM"}
    config = {PLATFORM_ZEPHYR: zephyr_config}

    def fake_schema(cfg):
        CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NATIVESIM")
        return cfg

    with patch(
        "esphome.components.zephyr.CONFIG_SCHEMA", side_effect=fake_schema
    ) as mock_schema:
        result = upload_program(config, object(), "some_host")

    mock_schema.assert_called_once_with(zephyr_config)
    assert CORE.data[KEY_ZEPHYR]["variant"] == "NATIVESIM"
    # NATIVESIM isn't esp32-family -- no uploader for it yet, but the repopulation
    # above is what let zephyr_variant_family() run at all instead of KeyError-ing.
    assert result is False


def test_upload_program_raises_when_zephyr_config_missing_from_cache() -> None:
    CORE.data.pop(KEY_ZEPHYR, None)
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    config = {}  # no PLATFORM_ZEPHYR block to re-validate against

    with pytest.raises(EsphomeError, match="re-validate and recompile"):
        upload_program(config, object(), "some_host")


# ---------------------------------------------------------------------------
# _resolve_shield_source / _resolve_snippet_source -- board_root fallback must
# still be validated against, not just an explicit shield_source:/snippet_source:
# ---------------------------------------------------------------------------


def test_resolve_shield_source_returns_none_without_board_root() -> None:
    # No shield_source: and no board_source: (board_root=None) either -- nothing
    # to validate against, defers to DTS validation against the SDK's own tree.
    assert _resolve_shield_source({}, ["nrf7002ek"], None) is None


def test_resolve_shield_source_validates_against_board_root_fallback(
    tmp_path: Path,
) -> None:
    (tmp_path / "boards" / "shields" / "nrf7002ek").mkdir(parents=True)
    (tmp_path / "boards" / "shields" / "nrf7002ek" / "shield.yml").write_text(
        "shield:\n  name: nrf7002ek\n"
    )

    result = _resolve_shield_source({}, ["nrf7002ek"], tmp_path)
    assert result == tmp_path


def test_resolve_shield_source_raises_for_missing_shield_under_board_root(
    tmp_path: Path,
) -> None:
    with pytest.raises(cv.Invalid, match="Could not find boards/shields"):
        _resolve_shield_source({}, ["no_such_shield"], tmp_path)


def test_resolve_snippet_source_raises_for_missing_snippet_under_board_root(
    tmp_path: Path,
) -> None:
    with pytest.raises(cv.Invalid, match="Could not find snippets"):
        _resolve_snippet_source({}, ["no_such_snippet"], tmp_path)


# ---------------------------------------------------------------------------
# _resolve_board_source -- board.yml lookup, including "@<revision>" validation
# ---------------------------------------------------------------------------


def _local_board_source_config(tmp_path: Path) -> dict:
    CORE.config_path = tmp_path / "test.yaml"
    return {
        CONF_BOARD_SOURCE: {CONF_TYPE: TYPE_LOCAL, CONF_PATH: "."},
    }


def _write_revisioned_board_yml(tmp_path: Path) -> None:
    board_dir = tmp_path / "boards" / "acme" / "my_board"
    board_dir.mkdir(parents=True)
    (board_dir / "board.yml").write_text(
        "board:\n"
        "  name: my_board\n"
        "  vendor: acme\n"
        "  revision:\n"
        "    format: major.minor.patch\n"
        "    revisions:\n"
        "      - name: '1.0.0'\n"
        "      - name: '2.0.0'\n"
    )


def test_resolve_board_source_raises_when_board_yml_missing(tmp_path: Path) -> None:
    config = _local_board_source_config(tmp_path)
    with pytest.raises(cv.Invalid, match="Could not find boards"):
        _resolve_board_source(config, "no_such_board")


def test_resolve_board_source_accepts_board_without_revision(tmp_path: Path) -> None:
    (tmp_path / "boards" / "acme" / "my_board").mkdir(parents=True)
    (tmp_path / "boards" / "acme" / "my_board" / "board.yml").write_text(
        "board:\n  name: my_board\n  vendor: acme\n"
    )
    config = _local_board_source_config(tmp_path)
    result = _resolve_board_source(config, "my_board")
    assert result == tmp_path


def test_resolve_board_source_accepts_valid_revision(tmp_path: Path) -> None:
    _write_revisioned_board_yml(tmp_path)
    config = _local_board_source_config(tmp_path)
    result = _resolve_board_source(config, "my_board@1.0.0")
    assert result == tmp_path


def test_resolve_board_source_accepts_closest_lower_revision(tmp_path: Path) -> None:
    _write_revisioned_board_yml(tmp_path)
    config = _local_board_source_config(tmp_path)
    result = _resolve_board_source(config, "my_board@1.5.0")
    assert result == tmp_path


def test_resolve_board_source_raises_for_revision_below_lowest_declared(
    tmp_path: Path,
) -> None:
    _write_revisioned_board_yml(tmp_path)
    config = _local_board_source_config(tmp_path)
    with pytest.raises(cv.Invalid, match="not valid for board"):
        _resolve_board_source(config, "my_board@0.1.0")


def test_resolve_board_source_raises_for_revision_on_board_without_revisions(
    tmp_path: Path,
) -> None:
    (tmp_path / "boards" / "acme" / "my_board").mkdir(parents=True)
    (tmp_path / "boards" / "acme" / "my_board" / "board.yml").write_text(
        "board:\n  name: my_board\n  vendor: acme\n"
    )
    config = _local_board_source_config(tmp_path)
    with pytest.raises(cv.Invalid, match="does not declare any revisions"):
        _resolve_board_source(config, "my_board@1.0.0")


# ---------------------------------------------------------------------------
# request_zephyr_module / zephyr_set_module_override / resolve_zephyr_modules --
# real registered NRF52 variant (NCS.modules == {"zigbee": NCS_ZIGBEE_TEMPLATE}) used
# throughout rather than a fake, since the point is exercising the real capability
# table.
# ---------------------------------------------------------------------------


def _set_framework_version(version: cv.Version) -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_ESP32,
        KEY_FRAMEWORK_VERSION: version,
    }


def test_request_zephyr_module_records_template_for_available_capability() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    request_zephyr_module("zigbee")
    requests = CORE.data[KEY_ZEPHYR]["module_requests"]
    assert requests["zigbee"].name == "ncs-zigbee"


def test_request_zephyr_module_raises_for_unavailable_capability() -> None:
    # framework: type: zephyr (mainline) has no "zigbee" module -- only ncs does.
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="zephyr")
    with pytest.raises(EsphomeError, match="isn't available"):
        request_zephyr_module("zigbee")


def test_resolve_zephyr_modules_uses_template_default_version() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    _set_framework_version(cv.Version(3, 4, 0))
    request_zephyr_module("zigbee")

    resolved = resolve_zephyr_modules()

    assert len(resolved) == 1
    assert resolved[0].name == "ncs-zigbee"
    assert resolved[0].revision == "v1.4.0"


def test_resolve_zephyr_modules_raises_without_override_or_table_match() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    _set_framework_version(cv.Version(3, 9, 0))  # no (3, 9) entry in default_versions
    request_zephyr_module("zigbee")

    with pytest.raises(EsphomeError, match="no known compatible version"):
        resolve_zephyr_modules()


def test_resolve_zephyr_modules_version_override_avoids_table_miss() -> None:
    # The scenario a plain table-miss error would otherwise block: a root SDK version
    # with no known-compatible module version still works once the user names one
    # explicitly via zephyr: modules:, without needing to touch the request itself.
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    _set_framework_version(cv.Version(3, 9, 0))
    request_zephyr_module("zigbee")
    zephyr_set_module_override("ncs-zigbee", "1.9.9")

    resolved = resolve_zephyr_modules()

    assert resolved[0].revision == "v1.9.9"


def test_resolve_zephyr_modules_full_module_override_replaces_template() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    _set_framework_version(cv.Version(3, 4, 0))
    request_zephyr_module("zigbee")
    custom = ZephyrModule(
        name="ncs-zigbee", manifest_url="https://example.com/fork", revision="my-branch"
    )
    zephyr_set_module_override("ncs-zigbee", custom)

    resolved = resolve_zephyr_modules()

    assert resolved == [custom]


def test_resolve_zephyr_modules_unrequested_override_without_source_raises() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    _set_framework_version(cv.Version(3, 4, 0))
    zephyr_set_module_override("mystery", "1.0.0")

    with pytest.raises(EsphomeError, match="wasn't requested by any component"):
        resolve_zephyr_modules()


def test_resolve_zephyr_modules_empty_when_nothing_requested() -> None:
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    _set_framework_version(cv.Version(3, 4, 0))

    assert resolve_zephyr_modules() == []


def test_resolve_zephyr_modules_raises_when_two_capabilities_collide_on_module_name() -> (
    None
):
    # Two different capability keys resolving to templates that share a module name
    # but aren't the same template -- must not silently let one clobber the other.
    CORE.data[KEY_ZEPHYR] = _empty_zephyr_data(variant="NRF52", framework_type="ncs")
    _set_framework_version(cv.Version(3, 4, 0))
    requests = CORE.data[KEY_ZEPHYR]["module_requests"]
    requests["zigbee"] = ZephyrModuleTemplate(
        name="shared-name", manifest_url="http://a", default_versions={(3, 4): "1.0.0"}
    )
    requests["other"] = ZephyrModuleTemplate(
        name="shared-name", manifest_url="http://b", default_versions={(3, 4): "2.0.0"}
    )

    with pytest.raises(EsphomeError, match="both resolve to 'shared-name'"):
        resolve_zephyr_modules()


# ---------------------------------------------------------------------------
# _MODULE_SCHEMA -- zephyr: modules: entries
# ---------------------------------------------------------------------------


def test_module_schema_accepts_name_and_git_source() -> None:
    result = _MODULE_SCHEMA(
        {
            CONF_NAME: "my_module",
            CONF_SOURCE: {CONF_TYPE: TYPE_GIT, CONF_URL: "https://example.com/mod"},
        }
    )
    assert result[CONF_NAME] == "my_module"
    assert result[CONF_SOURCE][CONF_URL] == "https://example.com/mod"


def test_module_schema_defaults_git_ref_to_main() -> None:
    result = _MODULE_SCHEMA(
        {
            CONF_NAME: "my_module",
            CONF_SOURCE: {CONF_TYPE: TYPE_GIT, CONF_URL: "https://example.com/mod"},
        }
    )
    assert result[CONF_SOURCE]["ref"] == "main"


def test_module_schema_accepts_name_and_version_only() -> None:
    result = _MODULE_SCHEMA({CONF_NAME: "ncs-zigbee", CONF_VERSION: "1.4.2"})
    assert result[CONF_VERSION] == "1.4.2"
    assert CONF_SOURCE not in result


def test_module_schema_accepts_local_source(tmp_path: Path) -> None:
    CORE.config_path = tmp_path / "test.yaml"
    result = _MODULE_SCHEMA(
        {
            CONF_NAME: "my_local_module",
            CONF_SOURCE: {CONF_TYPE: TYPE_LOCAL, CONF_PATH: "."},
        }
    )
    assert result[CONF_SOURCE][CONF_PATH] == tmp_path


def test_module_schema_rejects_name_alone() -> None:
    with pytest.raises(cv.Invalid, match="at least one"):
        _MODULE_SCHEMA({CONF_NAME: "bare"})
