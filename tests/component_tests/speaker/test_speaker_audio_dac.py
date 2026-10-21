"""Tests for the speaker audio_dac option."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_speaker_empty_audio_dac_uses_the_only_dac(
    generate_main: Callable[[str | Path], str],
) -> None:
    """An empty audio_dac: still picks the only audio DAC in the config."""
    main_cpp = generate_main(
        "tests/component_tests/speaker/test_speaker_audio_dac_empty.yaml"
    )

    assert "speaker_audio_dacs[] PROGMEM = {only_dac};" in main_cpp
    assert "speaker_empty_audio_dac->set_audio_dacs(speaker_audio_dacs, 1);" in main_cpp


def test_speaker_audio_dac_list_single_and_absent(
    generate_main: Callable[[str | Path], str],
) -> None:
    """A list feeds every DAC, a single id one DAC, and no audio_dac sets nothing."""
    main_cpp = generate_main(
        "tests/component_tests/speaker/test_speaker_audio_dac_forms.yaml"
    )

    assert "speaker_audio_dacs[] PROGMEM = {dac_a, dac_b};" in main_cpp
    assert "speaker_list->set_audio_dacs(speaker_audio_dacs, 2);" in main_cpp
    assert "speaker_audio_dacs_2[] PROGMEM = {dac_a};" in main_cpp
    assert "source_single->set_audio_dacs(speaker_audio_dacs_2, 1);" in main_cpp
    assert "source_absent->set_audio_dac" not in main_cpp
