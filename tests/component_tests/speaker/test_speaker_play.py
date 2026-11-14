"""Tests for the speaker.play action codegen."""

from collections.abc import Callable
from pathlib import Path


def test_speaker_play_constant_and_lambda_payloads(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Identical constant payloads share one flash table and are played from it;
    a lambda's vector is played as its data and size."""
    main_cpp = generate_main("tests/component_tests/speaker/test_speaker_play.yaml")

    assert main_cpp.count("[] PROGMEM = {0x10, 0x20, 0x30, 0x40};") == 1
    assert main_cpp.count("::speaker_id->play(::esphome__speaker_play__bytes, 4);") == 2
    assert (
        "::speaker_id->play(esphome__bytes.data(), esphome__bytes.size());" in main_cpp
    )
