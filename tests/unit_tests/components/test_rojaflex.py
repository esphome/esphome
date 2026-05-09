"""Tests for rojaflex component validation."""

from unittest.mock import patch

from esphome import config, yaml_util
from esphome.components import rojaflex
from esphome.config_validation import Invalid
from esphome.core import CORE


VALID_BASE_CONFIG = """
esphome:
  name: test-rojaflex

esp32:
  board: esp32dev
  framework:
    type: arduino

spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

cc1101:
  id: transceiver
  cs_pin: GPIO5
  gdo0_pin: GPIO4
  frequency: 433.92MHz
  if_frequency: 153kHz
  filter_bandwidth: 203kHz
  channel: 0
  channel_spacing: 200kHz
  symbol_rate: 9992.6
  modulation_type: GFSK
  packet_mode: true
  packet_length: 9
  crc_enable: false
  whitening: false
  sync_mode: "16/16"
  sync0: 0x91
  sync1: 0xD3
  num_preamble: 2

rojaflex:
  id: rojaflex_core
  cc1101_id: transceiver
  housecode: "A1B2C3D"
  tx_repetitions: 2

cover:
  - platform: rojaflex
    name: "Shutter ID1"
    rojaflex_id: rojaflex_core
    channel: 1
"""


def _read_config_from_text(tmp_path, content):
    test_file = tmp_path / "test.yaml"
    test_file.write_text(content)
    parsed_yaml = yaml_util.load_yaml(test_file)

    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed_yaml),
        patch.object(CORE, "config_path", test_file),
    ):
        return config.read_config({})


def test_validate_housecode_uppercases():
    assert rojaflex.validate_housecode("a1b2c3d") == "A1B2C3D"


def test_validate_housecode_invalid_length():
    with patch.object(CORE, "vscode", False):
        try:
            rojaflex.validate_housecode("123")
        except Invalid as err:
            assert "exactly 7 hex chars" in str(err)
        else:
            raise AssertionError("Expected Invalid for short housecode")


def test_validate_housecode_invalid_chars():
    with patch.object(CORE, "vscode", False):
        try:
            rojaflex.validate_housecode("ZZZZZZZ")
        except Invalid as err:
            assert "must be hexadecimal" in str(err)
        else:
            raise AssertionError("Expected Invalid for non-hex housecode")


def test_rojaflex_valid_config_passes(tmp_path):
    result = _read_config_from_text(tmp_path, VALID_BASE_CONFIG)
    assert result is not None


def test_rojaflex_defaults_apply_when_optional_values_missing(tmp_path):
    config_without_optionals = VALID_BASE_CONFIG.replace('  housecode: "A1B2C3D"\n', "").replace(
        "  tx_repetitions: 2\n", ""
    )
    result = _read_config_from_text(tmp_path, config_without_optionals)
    assert result is not None
    assert result["rojaflex"]["housecode"] == "0000000"
    assert result["rojaflex"]["tx_repetitions"] == 2


def test_rojaflex_housecode_is_normalized_to_uppercase_in_config(tmp_path):
    lower_config = VALID_BASE_CONFIG.replace('housecode: "A1B2C3D"', 'housecode: "a1b2c3d"')
    result = _read_config_from_text(tmp_path, lower_config)
    assert result is not None
    assert result["rojaflex"]["housecode"] == "A1B2C3D"


def test_rojaflex_invalid_housecode_fails(tmp_path, capsys):
    bad_config = VALID_BASE_CONFIG.replace('housecode: "A1B2C3D"', 'housecode: "XYZ1234"')
    result = _read_config_from_text(tmp_path, bad_config)
    assert result is None
    captured = capsys.readouterr()
    assert "housecode must be hexadecimal" in captured.out


def test_rojaflex_tx_repetitions_lower_boundary_accepts_1(tmp_path):
    min_cfg = VALID_BASE_CONFIG.replace("tx_repetitions: 2", "tx_repetitions: 1")
    assert _read_config_from_text(tmp_path, min_cfg) is not None


def test_rojaflex_tx_repetitions_upper_boundary_accepts_9(tmp_path):
    max_cfg = VALID_BASE_CONFIG.replace("tx_repetitions: 2", "tx_repetitions: 9")
    assert _read_config_from_text(tmp_path, max_cfg) is not None


def test_rojaflex_tx_repetitions_rejects_out_of_range(tmp_path):
    below_min = VALID_BASE_CONFIG.replace("tx_repetitions: 2", "tx_repetitions: 0")
    above_max = VALID_BASE_CONFIG.replace("tx_repetitions: 2", "tx_repetitions: 10")
    assert _read_config_from_text(tmp_path, below_min) is None
    assert _read_config_from_text(tmp_path, above_max) is None


def test_rojaflex_cover_channel_lower_boundary_accepts_0(tmp_path):
    channel_0 = VALID_BASE_CONFIG.replace("channel: 1", "channel: 0")
    assert _read_config_from_text(tmp_path, channel_0) is not None


def test_rojaflex_cover_channel_upper_boundary_accepts_15(tmp_path):
    channel_15 = VALID_BASE_CONFIG.replace("channel: 1", "channel: 15")
    assert _read_config_from_text(tmp_path, channel_15) is not None


def test_rojaflex_cover_channel_rejects_16(tmp_path):
    bad_channel = VALID_BASE_CONFIG.replace("channel: 1", "channel: 16")
    assert _read_config_from_text(tmp_path, bad_channel) is None
