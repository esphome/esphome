"""Tests for the file image platform's prefetch extraction."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

from esphome.components.file import image as file_image
from esphome.external_files import RemoteFile
from esphome.loader import get_component, get_platform


def test_extract_mdi_shorthand(setup_core: Path) -> None:
    ref = file_image._extract_file_ref("mdi:home")
    assert ref is not None
    assert ref.url == file_image.MDI_SOURCES["mdi"] + "home.svg"
    assert ref.path.name == "home.svg"
    assert ref.path.parent.name == "mdi"


def test_extract_web_url(setup_core: Path) -> None:
    url = "https://example.com/img.png"
    ref = file_image._extract_file_ref(url)
    assert ref == RemoteFile(url, file_image.compute_local_image_path(url))


def test_extract_typed_dicts(setup_core: Path) -> None:
    url = "https://example.com/img.png"
    assert file_image._extract_file_ref({"source": "web", "url": url}) == RemoteFile(
        url, file_image.compute_local_image_path(url)
    )
    ref = file_image._extract_file_ref({"source": "mdil", "icon": "home"})
    assert ref is not None
    assert ref.url == file_image.MDI_SOURCES["mdil"] + "home.svg"


def test_extract_skips_local_and_garbage(setup_core: Path) -> None:
    assert file_image._extract_file_ref("images/local.png") is None
    assert file_image._extract_file_ref("mdi:not a valid icon!") is None
    assert file_image._extract_file_ref({"source": "local", "path": "x.png"}) is None
    assert file_image._extract_file_ref(42) is None
    assert file_image._extract_file_ref(None) is None


def test_prefetch_files_yields_remote_refs(setup_core: Path) -> None:
    entries = [
        {"file": "mdi:home"},
        {"file": "images/local.png"},
        {"file": "https://example.com/img.png"},
        {"no_file_key": True},
    ]
    [files] = list(file_image.PREFETCH_FILES(entries))
    assert len(files) == 2
    assert files[0].url.endswith("home.svg")
    assert files[1].url == "https://example.com/img.png"


def test_extractor_matches_validator_path(setup_core: Path) -> None:
    """The path the validator downloads to equals the extractor's path."""
    with patch(
        "esphome.components.file.image.external_files.download_content"
    ) as mock_download:
        file_image.validate_file_shorthand("mdi:home")

    validated_path = mock_download.call_args[0][1]
    assert validated_path == file_image._extract_file_ref("mdi:home").path


def test_hook_is_wired_to_both_animation_domains() -> None:
    """Both animation entry points expose the shared image hook."""

    assert get_component("animation").prefetch_files is file_image.PREFETCH_FILES
    assert (
        get_platform("image", "animation").prefetch_files is file_image.PREFETCH_FILES
    )
