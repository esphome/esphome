"""Tests for the gsl3670 touchscreen prefetch extraction."""

from __future__ import annotations

from pathlib import Path

from esphome.components.gsl3670 import touchscreen as gsl
from esphome.external_files import RemoteFile


def test_prefetch_explicit_url(setup_core: Path) -> None:
    url = "https://example.com/fw.bin"
    entries = [{"platform": "gsl3670", "firmware": {"url": url}}]
    assert list(gsl.PREFETCH_FILES(entries)) == [
        [RemoteFile(url, gsl._cache_path(url))]
    ]


def test_prefetch_model_default_firmware(setup_core: Path) -> None:
    entries = [{"platform": "gsl3670", "model": "seeed-reterminal-d1001"}]
    [files] = list(gsl.PREFETCH_FILES(entries))
    assert len(files) == 1
    assert (
        files[0].url == gsl.MODELS["SEEED-RETERMINAL-D1001"][gsl.CONF_FIRMWARE]["url"]
    )
    assert files[0].path == gsl._cache_path(files[0].url)


def test_prefetch_skips_local_file_and_custom(setup_core: Path) -> None:
    entries = [
        {"platform": "gsl3670", "firmware": {"file": "fw.bin"}},
        {"platform": "gsl3670", "model": "CUSTOM"},
        {"platform": "gsl3670"},
    ]
    assert list(gsl.PREFETCH_FILES(entries)) == [[]]
