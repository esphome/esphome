"""Tests for the i2s_audio full duplex bus."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest
import yaml

from esphome import config_validation as cv, final_validate as fv
from esphome.components.i2s_audio import _validate_full_duplex
from esphome.core import CORE, ID

BUS_ID = "duplex_bus"


def _device(**overrides: Any) -> dict[str, Any]:
    return {
        "platform": "i2s_audio",
        "id": ID("device", is_declaration=True),
        "i2s_audio_id": ID(BUS_ID, is_declaration=False),
        "i2s_dout_pin": 17,
        "channel": "stereo",
        "i2s_comm_fmt": "stand_i2s",
        "sample_rate": 48000,
        "bits_per_sample": 32,
        "i2s_mode": "primary",
        "use_apll": False,
        "mclk_multiple": 256,
    } | overrides


def _validate(
    microphones: list[dict[str, Any]], speakers: list[dict[str, Any]]
) -> None:
    token = fv.full_config.set({"microphone": microphones, "speaker": speakers})
    try:
        _validate_full_duplex(ID(BUS_ID))
    finally:
        fv.full_config.reset(token)


def test_full_duplex_generates_code(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The microphone and every speaker are registered with the bus and the full duplex code is enabled."""
    main_cpp = generate_main(
        "tests/component_tests/i2s_audio/test_full_duplex_multiple_speakers.yaml"
    )

    assert "duplex_bus->set_keep_alive(true);" in main_cpp
    assert "duplex_bus->set_audio_in(duplex_mic);" in main_cpp
    assert "duplex_bus->set_audio_out(duplex_speaker_a);" in main_cpp
    assert "duplex_bus->set_audio_out(duplex_speaker_b);" in main_cpp
    assert "USE_I2S_AUDIO_FULL_DUPLEX" in {define.name for define in CORE.defines}


_OTHER_BUS = _device(i2s_audio_id=ID("other_bus", is_declaration=False))


@pytest.mark.parametrize(
    ("microphones", "speakers"),
    [
        pytest.param([_device(pdm=False)], [_device(spdif_mode=False)], id="matching"),
        pytest.param(
            [_device(), _OTHER_BUS], [_device(), _OTHER_BUS], id="other-bus-ignored"
        ),
        pytest.param(
            [_device()], [_device(), _device(), _device()], id="several-speakers"
        ),
    ],
)
def test_full_duplex_accepts(
    microphones: list[dict[str, Any]], speakers: list[dict[str, Any]]
) -> None:
    """Devices with the same clock and TX settings share the bus; other buses do not count."""
    _validate(microphones, speakers)


@pytest.mark.parametrize(
    ("microphones", "speakers"),
    [
        ([], [_device()]),
        ([_device()], []),
        ([_device(), _device()], [_device()]),
    ],
)
def test_full_duplex_requires_one_microphone_and_a_speaker(
    microphones: list[dict[str, Any]], speakers: list[dict[str, Any]]
) -> None:
    """Exactly one microphone and at least one speaker must use the bus."""
    with pytest.raises(cv.Invalid, match="exactly one i2s_audio microphone"):
        _validate(microphones, speakers)


@pytest.mark.parametrize(
    ("microphone", "speaker", "error_match"),
    [
        pytest.param(_device(pdm=True), _device(), "PDM microphone", id="pdm"),
        pytest.param(_device(), _device(spdif_mode=True), "SPDIF speaker", id="spdif"),
        pytest.param(
            _device(),
            _device(i2s_comm_fmt="stand_msb"),
            "Philips I2S framing",
            id="msb",
        ),
    ],
)
def test_full_duplex_rejects_unsupported_devices(
    microphone: dict[str, Any], speaker: dict[str, Any], error_match: str
) -> None:
    """PDM receive and SPDIF output cannot share the standard mode clocks."""
    with pytest.raises(cv.Invalid, match=error_match):
        _validate([microphone], [speaker])


@pytest.mark.parametrize(
    ("key", "value"),
    [
        ("sample_rate", 16000),
        ("bits_per_sample", 16),
        ("i2s_mode", "secondary"),
        ("use_apll", True),
        ("mclk_multiple", 384),
    ],
)
def test_full_duplex_rejects_mismatched_clock_settings(key: str, value: Any) -> None:
    """Settings that shape the shared clocks must match."""
    with pytest.raises(cv.Invalid, match=f"must use the same '{key}'"):
        _validate([_device()], [_device(**{key: value})])


@pytest.mark.parametrize(
    ("key", "value"),
    [
        ("i2s_dout_pin", 18),
        ("channel", "mono"),
        ("i2s_comm_fmt", "stand_pcm_long"),
    ],
)
def test_full_duplex_rejects_mismatched_speakers(key: str, value: Any) -> None:
    """Settings that shape the TX channel must match across speakers."""
    with pytest.raises(
        cv.Invalid,
        match=f"speakers on 'full_duplex' bus '{BUS_ID}' must use the same '{key}'",
    ):
        _validate([_device()], [_device(), _device(**{key: value})])


def test_full_duplex_rejects_different_slot_layout() -> None:
    """Stereo RX and mono TX must not generate different word clocks."""
    with pytest.raises(cv.Invalid, match="same mono/stereo slot layout"):
        _validate([_device(channel="right")], [_device()])


@pytest.mark.parametrize("full_duplex", [False, True])
def test_optional_modes_codegen(
    generate_main: Callable[[str | Path], str], tmp_path: Path, full_duplex: bool
) -> None:
    """Keep-alive is opt-in; ordinary buses retain their original ownership."""
    source = Path(
        "tests/component_tests/i2s_audio/test_full_duplex_multiple_speakers.yaml"
    )
    config = tmp_path / "optional.yaml"
    config.write_text(
        source.read_text(encoding="utf-8")
        .replace("full_duplex: true", f"full_duplex: {str(full_duplex).lower()}")
        .replace("    keep_alive: true\n", ""),
        encoding="utf-8",
    )
    main_cpp = generate_main(config)
    assert (
        "USE_I2S_AUDIO_FULL_DUPLEX" in {define.name for define in CORE.defines}
    ) == full_duplex
    if full_duplex:
        assert "duplex_bus->set_keep_alive(false);" in main_cpp
    else:
        assert "duplex_bus->set_audio_in(" not in main_cpp
        assert "duplex_bus->set_keep_alive(" not in main_cpp


def test_keep_alive_without_duplex_is_invalid(tmp_path: Path) -> None:
    """A persistent RX clock requires the paired-channel ownership model."""
    from esphome.config import load_config

    source = Path(
        "tests/component_tests/i2s_audio/test_full_duplex_multiple_speakers.yaml"
    )
    config = tmp_path / "invalid.yaml"
    config.write_text(
        source.read_text(encoding="utf-8").replace(
            "full_duplex: true", "full_duplex: false"
        ),
        encoding="utf-8",
    )
    CORE.config_path = config
    result = load_config({})
    assert any(
        "'keep_alive' requires 'full_duplex: true'" in str(error)
        for error in result.errors
    )


@pytest.mark.parametrize("reverse_order", [False, True])
@pytest.mark.parametrize(
    ("sample_rate", "channels", "slot_bits", "valid"),
    [
        (16000, 1, 16, False),
        (44100, 1, 16, False),
        (48000, 2, 16, False),
        (48000, 1, 32, False),
        (48000, 1, 16, True),
    ],
)
def test_duplex_pipeline_format(
    tmp_path: Path,
    reverse_order: bool,
    sample_rate: int,
    channels: int,
    slot_bits: int,
    valid: bool,
) -> None:
    """Reject incompatible preferred formats regardless of YAML component order."""
    from esphome.config import load_config

    source = Path("tests/component_tests/i2s_audio/test_full_duplex_formats.yaml")
    config = yaml.safe_load(source.read_text(encoding="utf-8"))
    pipeline = config["media_player"][0]["announcement_pipeline"]
    pipeline.update(sample_rate=sample_rate, num_channels=channels)
    config["microphone"][0]["bits_per_sample"] = f"{slot_bits}bit"
    config["speaker"][0]["bits_per_sample"] = f"{slot_bits}bit"
    if reverse_order:
        config = dict(reversed(list(config.items())))
    path = tmp_path / "pipeline.yaml"
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    CORE.config_path = path
    result = load_config({})
    assert (not result.errors) == valid, result.errors
    if not valid:
        assert any(
            "Invalid configuration for the speaker media_player" in str(e)
            for e in result.errors
        ), result.errors


@pytest.mark.parametrize("full_duplex", [False, True])
def test_resampler_can_feed_fixed_duplex_bus(tmp_path: Path, full_duplex: bool) -> None:
    """A 16 kHz source is valid when a resampler converts it to the bus's 48 kHz."""
    from esphome.config import load_config

    source = Path("tests/component_tests/i2s_audio/test_full_duplex_formats.yaml")
    config = yaml.safe_load(source.read_text(encoding="utf-8"))
    config["i2s_audio"]["full_duplex"] = full_duplex
    config["i2s_audio"]["keep_alive"] = full_duplex
    config["speaker"].append(
        {
            "platform": "resampler",
            "id": "converted",
            "output_speaker": "output",
            "sample_rate": 48000,
        }
    )
    config["media_player"][0]["announcement_pipeline"].update(
        speaker="converted", sample_rate=16000
    )
    path = tmp_path / "resampled.yaml"
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    CORE.config_path = path
    result = load_config({})
    assert not result.errors, result.errors


def test_non_duplex_keeps_variable_format(tmp_path: Path) -> None:
    """The new limits do not restrict ordinary dynamically configured I2S speakers."""
    from esphome.config import load_config

    source = Path("tests/component_tests/i2s_audio/test_full_duplex_formats.yaml")
    config = yaml.safe_load(source.read_text(encoding="utf-8"))
    config["i2s_audio"].update(full_duplex=False, keep_alive=False)
    config["media_player"][0]["announcement_pipeline"].update(
        sample_rate=16000, num_channels=2
    )
    path = tmp_path / "non_duplex.yaml"
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    CORE.config_path = path
    result = load_config({})
    assert not result.errors, result.errors


@pytest.mark.parametrize("slot_bits", [24, 32])
def test_original_esp32_widens_duplex_stream(tmp_path: Path, slot_bits: int) -> None:
    """The original ESP32 converts 16-bit input to the slot's 32-bit memory width."""
    from esphome.config import load_config

    source = Path("tests/component_tests/i2s_audio/test_full_duplex_formats.yaml")
    config = yaml.safe_load(source.read_text(encoding="utf-8"))
    config["esp32"]["board"] = "esp32dev"
    for component in ("microphone", "speaker"):
        config[component][0].update(
            bits_per_sample=f"{slot_bits}bit", mclk_multiple=384
        )
    path = tmp_path / "widened.yaml"
    path.write_text(yaml.safe_dump(config, sort_keys=False))
    CORE.config_path = path
    result = load_config({})
    assert not result.errors, result.errors
