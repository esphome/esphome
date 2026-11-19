"""Tests for the audio_file component codegen."""

from collections.abc import Callable
from pathlib import Path


def test_audio_file_is_constant_in_flash(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The AudioFile is a global constant behind a const pointer, not a placement new."""
    main_cpp = generate_main(component_config_path("audio_file.yaml"))

    assert (
        "static constexpr audio::AudioFile audio_audiofile_id = audio::AudioFile{"
        in main_cpp
    )
    assert (
        "static const audio::AudioFile *const chime = &audio_audiofile_id;" in main_cpp
    )
    assert "new(chime)" not in main_cpp
    assert 'audio_file::add_named_audio_file(chime, "chime");' in main_cpp
