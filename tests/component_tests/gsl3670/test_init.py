"""Tests for the gsl3670 touchscreen configuration validation."""

from __future__ import annotations

from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.gsl3670 import touchscreen as gsl
from esphome.const import (
    CONF_CALIBRATION,
    CONF_INTERRUPT_PIN,
    CONF_MODEL,
    CONF_RESET_PIN,
    CONF_TRANSFORM,
    PlatformFramework,
)
from tests.component_tests.types import SetCoreConfigCallable

VALID_URL = "https://example.com/fw.bin"


def _make_firmware(blocks: int = 2) -> bytes:
    """Build a structurally valid firmware blob with ``blocks`` blocks.

    Each block is ``_FW_BLK_SIZE`` bytes: a 4-byte header (page address <= 0xEF
    followed by the 1/2/3 marker bytes) and a 128-byte payload.
    """
    out = bytearray()
    for i in range(blocks):
        out += bytes([i, 1, 2, 3]) + bytes(gsl._FW_BLK_SIZE - 4)
    return bytes(out)


def _write_firmware(tmp_path: Path, data: bytes | None = None) -> Path:
    """Write firmware bytes to a temp file and return its path."""
    path = tmp_path / "fw.bin"
    path.write_bytes(_make_firmware() if data is None else data)
    return path


# ---------------------------------------------------------------------------
# _validate_firmware_data - blob structure
# ---------------------------------------------------------------------------


def test_validate_firmware_data_accepts_valid_blob() -> None:
    """A correctly structured blob passes validation."""
    gsl._validate_firmware_data(_make_firmware(3), "test")


@pytest.mark.parametrize("length", [0, gsl._FW_BLK_SIZE - 1, gsl._FW_BLK_SIZE + 1])
def test_validate_firmware_data_rejects_bad_length(length: int) -> None:
    """The blob length must be a non-zero multiple of the block size."""
    with pytest.raises(cv.Invalid, match="length is incorrect"):
        gsl._validate_firmware_data(bytes(length), "test")


@pytest.mark.parametrize(
    "index,value",
    [
        (0, 0xF0),  # page address must be <= 0xEF
        (1, 0x00),  # marker byte must be 1
        (2, 0x00),  # marker byte must be 2
        (3, 0x00),  # marker byte must be 3
    ],
)
def test_validate_firmware_data_rejects_corrupted_header(
    index: int, value: int
) -> None:
    """A block whose header bytes are wrong is reported as corrupted."""
    data = bytearray(_make_firmware(2))
    # Corrupt the header of the second block.
    data[gsl._FW_BLK_SIZE + index] = value
    with pytest.raises(cv.Invalid, match="Corrupted firmware at block 1"):
        gsl._validate_firmware_data(bytes(data), "test")


# ---------------------------------------------------------------------------
# _cache_path / firmware_path
# ---------------------------------------------------------------------------


def test_cache_path_is_deterministic_per_url(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """The cache path is derived from (and stable for) the URL."""
    monkeypatch.setattr(
        gsl.external_files, "compute_local_file_dir", lambda _: tmp_path
    )
    first = gsl._cache_path(VALID_URL)
    assert first == gsl._cache_path(VALID_URL)
    assert first != gsl._cache_path("https://example.com/other.bin")
    assert first.parent == tmp_path


def test_firmware_path_prefers_local_file(tmp_path: Path) -> None:
    """A ``file`` source is returned as-is, without consulting the cache."""
    path = _write_firmware(tmp_path)
    assert gsl.firmware_path({"file": path}) == path


def test_firmware_path_uses_cache_for_url(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A ``url`` source resolves to the cache path for that URL."""
    monkeypatch.setattr(
        gsl.external_files, "compute_local_file_dir", lambda _: tmp_path
    )
    assert gsl.firmware_path({"url": VALID_URL}) == gsl._cache_path(VALID_URL)


# ---------------------------------------------------------------------------
# _validate_firmware / FIRMWARE_SCHEMA
# ---------------------------------------------------------------------------


def test_firmware_requires_exactly_one_source(tmp_path: Path) -> None:
    """Supplying both, or neither, of url/file is an error."""
    path = _write_firmware(tmp_path)
    with pytest.raises(cv.Invalid, match="Exactly one"):
        gsl._validate_firmware({"url": VALID_URL, "file": path})
    with pytest.raises(cv.Invalid, match="Exactly one"):
        gsl._validate_firmware({})


def test_firmware_file_valid(tmp_path: Path) -> None:
    """A valid firmware file passes the full FIRMWARE_SCHEMA."""
    path = _write_firmware(tmp_path)
    result = gsl.FIRMWARE_SCHEMA({"file": str(path)})
    assert result["file"] == path


def test_firmware_file_corrupt_rejected(tmp_path: Path) -> None:
    """A file whose contents fail the structural check is rejected."""
    path = _write_firmware(tmp_path, data=b"\x00" * (gsl._FW_BLK_SIZE * 2))
    with pytest.raises(cv.Invalid, match="Corrupted firmware"):
        gsl._validate_firmware({"file": path})


def test_firmware_url_downloads_and_validates(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A url source downloads the content and validates its structure."""
    data = _make_firmware()
    monkeypatch.setattr(
        gsl.external_files, "compute_local_file_dir", lambda _: tmp_path
    )
    monkeypatch.setattr(gsl.external_files, "download_content", lambda url, path: data)
    assert gsl._validate_firmware({"url": VALID_URL}) == {"url": VALID_URL}


def test_firmware_url_sha256_mismatch_rejected(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """A configured SHA-256 that does not match the download is rejected."""
    data = _make_firmware()
    monkeypatch.setattr(
        gsl.external_files, "compute_local_file_dir", lambda _: tmp_path
    )
    monkeypatch.setattr(gsl.external_files, "download_content", lambda url, path: data)
    with pytest.raises(cv.Invalid, match="SHA-256 mismatch"):
        gsl._validate_firmware({"url": VALID_URL, "sha256": "00" * 32})


def test_firmware_url_invalid_structure_rejected(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    """Downloaded content that is not a valid blob is rejected."""
    monkeypatch.setattr(
        gsl.external_files, "compute_local_file_dir", lambda _: tmp_path
    )
    monkeypatch.setattr(
        gsl.external_files, "download_content", lambda url, path: b"\x00\x01\x02"
    )
    with pytest.raises(cv.Invalid, match="length is incorrect"):
        gsl._validate_firmware({"url": VALID_URL})


# ---------------------------------------------------------------------------
# CONFIG_SCHEMA
# ---------------------------------------------------------------------------


@pytest.fixture(autouse=True)
def _esp32_core(set_core_config: SetCoreConfigCallable) -> None:
    """Configure the core as an ESP32 target for the schema tests."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )


def test_config_custom_model_minimal(tmp_path: Path) -> None:
    """The CUSTOM model validates with an explicit firmware file and pins."""
    fw = _write_firmware(tmp_path)
    result = gsl.CONFIG_SCHEMA(
        {
            "model": "custom",
            "interrupt_pin": 16,
            "reset_pin": 4,
            "firmware": {"file": str(fw)},
        }
    )
    assert result[CONF_MODEL] == "CUSTOM"
    assert "id" in result
    # The CUSTOM model supplies no transform/calibration defaults.
    assert CONF_TRANSFORM not in result
    assert CONF_CALIBRATION not in result


def test_config_custom_model_requires_firmware() -> None:
    """The firmware option is required for the CUSTOM model (no default)."""
    with pytest.raises(cv.Invalid, match=r"required key not provided.*firmware"):
        gsl.CONFIG_SCHEMA({"model": "custom", "interrupt_pin": 16, "reset_pin": 4})


def test_config_invalid_model_rejected() -> None:
    """An unknown model name is rejected."""
    with pytest.raises(cv.Invalid, match="model"):
        gsl.CONFIG_SCHEMA({"model": "nonexistent"})


def test_config_seeed_model_applies_defaults(tmp_path: Path) -> None:
    """The SEEED model populates transform and calibration defaults.

    ``reset_pin`` is overridden with a plain GPIO so the test does not depend on
    the model's default IO-expander pin.
    """
    fw = _write_firmware(tmp_path)
    result = gsl.CONFIG_SCHEMA(
        {
            "model": "seeed-reterminal-d1001",
            "reset_pin": 4,
            "firmware": {"file": str(fw)},
        }
    )
    assert result[CONF_MODEL] == "SEEED-RETERMINAL-D1001"
    # Transform defaults from the model.
    assert result[CONF_TRANSFORM] == {
        "swap_xy": True,
        "mirror_x": True,
        "mirror_y": True,
    }
    # Calibration defaults from the model.
    assert result[CONF_CALIBRATION]["x_min"] == 20
    assert result[CONF_CALIBRATION]["x_max"] == 872
    assert result[CONF_CALIBRATION]["y_min"] == 20
    assert result[CONF_CALIBRATION]["y_max"] == 1644
    # The interrupt pin default (16) is applied without being specified.
    assert CONF_INTERRUPT_PIN in result
    assert CONF_RESET_PIN in result


def test_config_rejects_non_dict() -> None:
    """A non-dict configuration is rejected."""
    with pytest.raises(cv.Invalid, match="expected a dictionary"):
        gsl.CONFIG_SCHEMA("not a dict")
