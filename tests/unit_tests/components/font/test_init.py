"""Tests for the font component's prefetch extraction."""

from __future__ import annotations

from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome import external_files
from esphome.components import font
import esphome.config_validation as cv
from esphome.external_files import RemoteFile


def _gspec(family: str, weight: int = 400, italic: bool = False) -> dict:
    return {"family": family, "weight": weight, "italic": italic}


def test_extract_gfonts_shorthand_defaults(setup_core: Path) -> None:
    spec = font._extract_remote_font("gfonts://Roboto")
    assert spec is not None
    assert spec[font.CONF_FAMILY] == "Roboto"
    assert spec[font.CONF_WEIGHT] == 400
    assert spec[font.CONF_ITALIC] is False


def test_extract_gfonts_shorthand_weight_variants(setup_core: Path) -> None:
    assert font._extract_remote_font("gfonts://Roboto@bold")[font.CONF_WEIGHT] == 700
    assert font._extract_remote_font("gfonts://Roboto@500")[font.CONF_WEIGHT] == 500


def test_extract_gfonts_normalizes_quoted_italic(setup_core: Path) -> None:
    """Boolean spellings the schema accepts are accepted by the extractor."""
    spec = font._extract_remote_font(
        {"type": "gfonts", "family": "Roboto", "italic": "true"}
    )
    assert spec is not None
    assert spec[font.CONF_ITALIC] is True
    assert (
        font._extract_remote_font(
            {"type": "gfonts", "family": "Roboto", "italic": "maybe"}
        )
        is None
    )


def test_extract_typed_gfonts_dict(setup_core: Path) -> None:
    spec = font._extract_remote_font(
        {"type": "gfonts", "family": "Roboto", "weight": "medium", "italic": True}
    )
    assert spec is not None
    assert spec[font.CONF_WEIGHT] == 500
    assert spec[font.CONF_ITALIC] is True


def test_extract_web_font(setup_core: Path) -> None:
    url = "https://example.com/font.ttf"
    for value in (url, {"type": "web", "url": url}):
        spec = font._extract_remote_font(value)
        assert spec is not None
        assert spec[font.CONF_URL] == url


def test_extract_skips_local_and_garbage(setup_core: Path) -> None:
    assert font._extract_remote_font("fonts/local.ttf") is None
    assert font._extract_remote_font({"type": "local", "path": "x.ttf"}) is None
    assert (
        font._extract_remote_font({"type": "gfonts", "family": "R", "weight": "no"})
        is None
    )
    assert font._extract_remote_font(42) is None


def test_prefetch_yields_css_for_stale_gfont(setup_core: Path) -> None:
    entries = [
        {"file": "gfonts://Roboto"},
        {"file": "fonts/local.ttf"},
        {
            "file": "https://example.com/font.ttf",
            "extras": [{"file": "gfonts://Monocraft"}],
        },
    ]
    batches = list(font.PREFETCH_FILES(entries))
    urls = [file.url for file in batches[0]]
    assert font._gfonts_css_url(_gspec("Roboto")) in urls
    assert font._gfonts_css_url(_gspec("Monocraft")) in urls
    assert "https://example.com/font.ttf" in urls
    assert len(batches[0]) == 3


def test_prefetch_skips_recent_ttf(setup_core: Path) -> None:
    path = font._gfonts_ttf_path(_gspec("Roboto"))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"cached ttf")

    batches = list(font.PREFETCH_FILES([{"file": "gfonts://Roboto"}]))
    assert batches == [[], []]


def test_stage2_parses_cached_css(setup_core: Path) -> None:

    css_path = font._gfonts_css_path(_gspec("Roboto"))
    css_path.parent.mkdir(parents=True, exist_ok=True)
    css_path.write_text(
        "src: url(https://fonts.gstatic.com/roboto.ttf) format('truetype');"
    )
    # Stage two only trusts CSS confirmed fetched this run.
    external_files._run_data().fresh_paths.add(css_path)

    batches = list(font.PREFETCH_FILES([{"file": "gfonts://Roboto"}]))
    assert batches[1] == [
        RemoteFile(
            "https://fonts.gstatic.com/roboto.ttf",
            font._gfonts_ttf_path(_gspec("Roboto")),
        )
    ]


def test_stage2_skips_missing_css(setup_core: Path) -> None:
    batches = list(font.PREFETCH_FILES([{"file": "gfonts://NoCss"}]))
    assert batches[1] == []


def test_prefetch_handles_bare_mapping_extras(setup_core: Path) -> None:
    """A bare-mapping extras value (valid raw config) is scanned."""
    entries = [
        {
            "file": "fonts/local.ttf",
            "extras": {"file": "gfonts://Roboto", "glyphs": "ABC"},
        }
    ]
    batches = list(font.PREFETCH_FILES(entries))
    assert [file.url for file in batches[0]] == [font._gfonts_css_url(_gspec("Roboto"))]


def test_unparseable_gfonts_css_is_evicted(setup_core: Path) -> None:
    """A CSS body that fails to parse is removed from the cache."""

    spec = {
        "family": "Roboto",
        "weight": 400,
        "italic": False,
        "refresh": font._REFRESH_VALIDATOR("0s"),
    }
    css_path = font._gfonts_css_path(spec)
    with (
        patch(
            "esphome.components.font.external_files.download_content",
            return_value=b"no truetype url here",
        ),
        patch(
            "esphome.components.font.external_files.is_fresh_this_run",
            return_value=True,
        ),
        pytest.raises(cv.Invalid, match="please report this"),
    ):
        font.download_gfont(spec)
    assert not css_path.exists()

    with (
        patch(
            "esphome.components.font.external_files.download_content",
            return_value=b"\xff\xfe\x00\x01binary",
        ),
        patch(
            "esphome.components.font.external_files.is_fresh_this_run",
            return_value=True,
        ),
        pytest.raises(cv.Invalid, match="not a text document"),
    ):
        font.download_gfont(spec)
    assert not css_path.exists()


def test_unrevalidated_gfonts_css_uses_cached_font(setup_core: Path) -> None:
    """A CSS body that could not be revalidated is not parsed for a ttf
    URL; the cached font is used instead."""
    spec = {
        "family": "Roboto",
        "weight": 400,
        "italic": False,
        "refresh": font._REFRESH_VALIDATOR("0s"),
    }
    ttf_path = font._gfonts_ttf_path(spec)
    ttf_path.parent.mkdir(parents=True, exist_ok=True)
    ttf_path.write_bytes(b"cached ttf")
    cache = MagicMock()
    with (
        patch.object(font, "FONT_CACHE", cache),
        patch(
            "esphome.components.font.external_files.download_content",
            return_value=b"stale css",
        ),
    ):
        assert font.download_gfont(spec) is spec
    cache.__setitem__.assert_called_once_with(spec, ttf_path)


def test_unrevalidated_gfonts_css_without_cached_font_errors(
    setup_core: Path,
) -> None:
    """No verified CSS and no cached font is a clear error."""
    spec = {
        "family": "Roboto",
        "weight": 500,
        "italic": False,
        "refresh": font._REFRESH_VALIDATOR("0s"),
    }
    with (
        patch(
            "esphome.components.font.external_files.download_content",
            return_value=b"stale css",
        ),
        pytest.raises(cv.Invalid, match="no cached font"),
    ):
        font.download_gfont(spec)


def test_stage2_skips_css_not_fetched_this_run(setup_core: Path) -> None:
    """A leftover CSS from an earlier run is not trusted for stage two."""
    css_path = font._gfonts_css_path(_gspec("Roboto"))
    css_path.parent.mkdir(parents=True, exist_ok=True)
    css_path.write_text(
        "src: url(https://fonts.gstatic.com/rotated.ttf) format('truetype');"
    )

    batches = list(font.PREFETCH_FILES([{"file": "gfonts://Roboto"}]))
    assert batches[1] == []
