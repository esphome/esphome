"""Tests for the shelly_dimmer firmware download and prefetch extraction."""

from __future__ import annotations

import hashlib
from pathlib import Path
from unittest.mock import patch

import pytest

from esphome import external_files
from esphome.components.shelly_dimmer import light as shd
from esphome.config_validation import Invalid
from esphome.external_files import RemoteFile


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def test_prefetch_known_version(setup_core: Path) -> None:
    entries = [{"firmware": {"version": "51.6", "update": True}}]
    stages = list(shd.PREFETCH_FILES(entries))
    url, sha = shd.KNOWN_FIRMWARE["51.6"]
    assert stages == [[RemoteFile(url, shd._firmware_cache_path(sha))]]


def test_prefetch_normalizes_update_like_the_schema(setup_core: Path) -> None:
    """Quoted booleans behave as the schema will normalize them."""
    url, sha = shd.KNOWN_FIRMWARE["51.6"]
    off = [{"firmware": {"version": "51.6", "update": "false"}}]
    assert list(shd.PREFETCH_FILES(off)) == [[]]
    on = [{"firmware": {"version": "51.6", "update": "true"}}]
    assert list(shd.PREFETCH_FILES(on)) == [
        [RemoteFile(url, shd._firmware_cache_path(sha))]
    ]


def test_prefetch_rejects_malformed_sha256(setup_core: Path) -> None:
    """A raw sha256 that is not a hash never becomes a path component."""
    entries = [
        {
            "firmware": {
                "url": "https://example.com/fw.bin",
                "sha256": "/tmp/payload",
                "update": True,
            }
        }
    ]
    assert list(shd.PREFETCH_FILES(entries)) == [[]]


def test_prefetch_skips_content_addressed_blob_on_disk(setup_core: Path) -> None:
    """A sha-keyed cache file needs no revalidation; get_firmware hashes it."""
    url, sha = shd.KNOWN_FIRMWARE["51.6"]
    shd._firmware_cache_path(sha).write_bytes(b"pinned firmware")
    entries = [{"firmware": {"version": "51.6", "update": True}}]
    assert list(shd.PREFETCH_FILES(entries)) == [[]]


def test_prefetch_explicit_url_without_sha(setup_core: Path) -> None:
    url = "https://example.com/fw.bin"
    entries = [{"firmware": {"url": url, "update": True}}]
    stages = list(shd.PREFETCH_FILES(entries))
    key = external_files.url_cache_key(url)
    # No sha means the bytes cannot be verified, so the prefetch itself
    # must carry the validator's strict no-stale policy.
    assert stages == [
        [RemoteFile(url, shd._firmware_cache_path(key), allow_stale=False)]
    ]


def test_prefetch_skips_no_update(setup_core: Path) -> None:
    entries = [
        {"firmware": {"version": "51.6"}},
        {"firmware": "51.6"},
        {"firmware": {"version": "0.0", "update": True}},
        {},
    ]
    assert list(shd.PREFETCH_FILES(entries)) == [[]]


def test_get_firmware_rejects_corrupted_cache(setup_core: Path) -> None:
    """A cached blob failing its hash check is discarded and re-downloaded."""
    good = b"good firmware"
    expected = _sha(good)
    path = shd._firmware_cache_path(expected)
    path.write_bytes(b"corrupted blob")

    with patch(
        "esphome.components.shelly_dimmer.light.external_files.download_content",
        return_value=good,
    ) as mock_download:
        result = shd.get_firmware(
            {
                "update": True,
                "url": "https://example.com/fw.bin",
                "sha256": expected,
            }
        )

    mock_download.assert_called_once()
    assert result == [int(b) for b in good]


def test_get_firmware_trusts_valid_cache(setup_core: Path) -> None:
    """A cached blob passing its hash check is used with zero network."""
    good = b"good firmware"
    expected = _sha(good)
    shd._firmware_cache_path(expected).write_bytes(good)

    with patch(
        "esphome.components.shelly_dimmer.light.external_files.download_content"
    ) as mock_download:
        result = shd.get_firmware(
            {
                "update": True,
                "url": "https://example.com/fw.bin",
                "sha256": expected,
            }
        )

    mock_download.assert_not_called()
    assert result == [int(b) for b in good]


def test_get_firmware_hash_mismatch_raises_and_uncaches(setup_core: Path) -> None:
    """A fresh download failing its hash check raises and is not cached."""
    expected = _sha(b"expected firmware")
    path = shd._firmware_cache_path(expected)

    with (
        patch(
            "esphome.components.shelly_dimmer.light.external_files.download_content",
            return_value=b"wrong firmware",
        ),
        pytest.raises(Invalid, match="Hash mismatch"),
    ):
        shd.get_firmware(
            {"update": True, "url": "https://example.com/fw.bin", "sha256": expected}
        )

    assert not path.exists()


def test_get_firmware_without_sha_rejects_stale(setup_core: Path) -> None:
    """The unverifiable no-hash branch must not accept a stale copy."""
    with patch(
        "esphome.components.shelly_dimmer.light.external_files.download_content",
        return_value=b"fw",
    ) as mock_download:
        shd.get_firmware({"update": True, "url": "https://example.com/fw.bin"})

    assert mock_download.call_args.kwargs["allow_stale"] is False
