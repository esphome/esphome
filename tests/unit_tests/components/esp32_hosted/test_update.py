"""Build-time firmware acquisition for embedded Hosted updates."""

from concurrent.futures import ThreadPoolExecutor
import hashlib
from pathlib import Path
import re
from threading import Barrier
from unittest.mock import AsyncMock, Mock

import pytest

from esphome import config_validation as cv
import esphome.codegen as cg
from esphome.components.esp32_hosted import update as hosted_update
from esphome.core import CORE
from esphome.types import ConfigType

FIRMWARE = b"test coprocessor firmware"
DIGEST = hashlib.sha256(FIRMWARE).hexdigest()
URL = "https://example.com/c6.bin"


@pytest.fixture(autouse=True)
def firmware_environment(tmp_path: Path, monkeypatch: pytest.MonkeyPatch) -> None:
    CORE.config_path = tmp_path / "device.yaml"
    monkeypatch.setenv("ESPHOME_DATA_DIR", str(tmp_path / ".esphome"))


@pytest.fixture
def downloader(monkeypatch: pytest.MonkeyPatch) -> Mock:
    def download(url: str, path: Path, **kwargs: object) -> bytes:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(FIRMWARE)
        return FIRMWARE

    mock = Mock(side_effect=download)
    monkeypatch.setattr("esphome.external_files.download_content", mock)
    return mock


def remote_config(digest: str = DIGEST) -> ConfigType:
    return {"type": "embedded", "url": URL, "sha256": digest}


def cache_file(digest: str = DIGEST) -> Path:
    return CORE.data_dir / "esp32_hosted" / "firmware" / f"{digest.lower()}.bin"


def embedded_schema(fields: ConfigType) -> ConfigType:
    return hosted_update.EMBEDDED_SCHEMA(fields | {"id": "c6_firmware"})


def test_existing_local_image(tmp_path: Path, downloader: Mock) -> None:
    image = tmp_path / "c6.bin"
    image.write_bytes(FIRMWARE)
    config = embedded_schema({"path": str(image), "sha256": DIGEST})
    hosted_update.FINAL_VALIDATE_SCHEMA(config | {"type": "embedded"})
    downloader.assert_not_called()


def test_url_schema_accepts_pinned_image() -> None:
    config = embedded_schema({"url": URL, "sha256": DIGEST})
    assert config["url"] == URL
    assert "path" not in config


@pytest.mark.parametrize("fields", [{}, {"path": "c6.bin", "url": URL}])
def test_exactly_one_source(tmp_path: Path, fields: ConfigType) -> None:
    (tmp_path / "c6.bin").write_bytes(FIRMWARE)
    with pytest.raises(cv.Invalid, match="one of path, url"):
        embedded_schema(fields | {"sha256": DIGEST})


@pytest.mark.parametrize("url", ["not a URL", "ftp://example.com/c6.bin"])
def test_invalid_url(url: str) -> None:
    with pytest.raises(cv.Invalid):
        embedded_schema({"url": url, "sha256": DIGEST})


def test_url_requires_hash() -> None:
    with pytest.raises(cv.Invalid):
        embedded_schema({"url": URL})


@pytest.mark.parametrize("digest", ["short", "g" * 64])
def test_invalid_hash(digest: str) -> None:
    with pytest.raises(cv.Invalid):
        embedded_schema({"url": URL, "sha256": digest})


def test_download_is_verified_and_cached(downloader: Mock) -> None:
    config = remote_config()
    hosted_update.FINAL_VALIDATE_SCHEMA(config)
    assert cache_file().read_bytes() == FIRMWARE
    assert config == remote_config()
    assert downloader.call_count == 1
    assert downloader.call_args.args[0] == URL


def test_verified_cache_never_accesses_network(downloader: Mock) -> None:
    image = cache_file()
    image.parent.mkdir(parents=True)
    image.write_bytes(FIRMWARE)
    downloader.side_effect = AssertionError("network must not be accessed")
    hosted_update.FINAL_VALIDATE_SCHEMA(remote_config(DIGEST.upper()))
    downloader.assert_not_called()


def test_corrupt_cache_is_replaced_by_verified_download(downloader: Mock) -> None:
    image = cache_file()
    image.parent.mkdir(parents=True)
    image.write_bytes(b"corrupted")
    hosted_update.FINAL_VALIDATE_SCHEMA(remote_config())
    assert image.read_bytes() == FIRMWARE
    downloader.assert_called_once()


def test_mismatched_download_is_not_published(downloader: Mock) -> None:
    wrong_digest = "0" * 64
    with pytest.raises(cv.Invalid, match="SHA256 mismatch"):
        hosted_update.FINAL_VALIDATE_SCHEMA(remote_config(wrong_digest))
    assert not cache_file(wrong_digest).exists()
    assert not list(cache_file().parent.glob("tmp*"))


@pytest.mark.parametrize("corrupt_cache", [False, True])
def test_download_failure_without_verified_cache(
    downloader: Mock, corrupt_cache: bool
) -> None:
    if corrupt_cache:
        image = cache_file()
        image.parent.mkdir(parents=True)
        image.write_bytes(b"truncated")
    downloader.side_effect = cv.Invalid(f"Could not download from {URL}: offline")
    with pytest.raises(cv.Invalid, match="offline"):
        hosted_update.FINAL_VALIDATE_SCHEMA(remote_config())


def test_same_url_with_new_hash_does_not_reuse_old_image(downloader: Mock) -> None:
    hosted_update.FINAL_VALIDATE_SCHEMA(remote_config())
    with pytest.raises(cv.Invalid, match="SHA256 mismatch"):
        hosted_update.FINAL_VALIDATE_SCHEMA(remote_config("0" * 64))
    assert downloader.call_count == 2
    assert cache_file().read_bytes() == FIRMWARE


def test_local_hash_mismatch(tmp_path: Path) -> None:
    image = tmp_path / "c6.bin"
    image.write_bytes(b"corrupted")
    with pytest.raises(cv.Invalid, match="SHA256 mismatch"):
        hosted_update.FINAL_VALIDATE_SCHEMA(
            {"type": "embedded", "path": image, "sha256": DIGEST}
        )


def test_interrupted_download_does_not_publish_partial_image(downloader: Mock) -> None:
    def interrupted(url: str, path: Path, **kwargs: object) -> bytes:
        path.write_bytes(FIRMWARE[:4])
        raise cv.Invalid("Download interrupted")

    downloader.side_effect = interrupted
    with pytest.raises(cv.Invalid, match="interrupted"):
        hosted_update.FINAL_VALIDATE_SCHEMA(remote_config())
    assert not cache_file().exists()
    assert not list(cache_file().parent.iterdir())


def test_concurrent_downloads_publish_only_verified_bytes(downloader: Mock) -> None:
    barrier = Barrier(2)

    def download(url: str, path: Path, **kwargs: object) -> bytes:
        path.write_bytes(FIRMWARE[:4])
        assert not cache_file().exists()
        barrier.wait(timeout=5)
        path.write_bytes(FIRMWARE)
        return FIRMWARE

    downloader.side_effect = download
    with ThreadPoolExecutor(max_workers=2) as executor:
        futures = [
            executor.submit(hosted_update.FINAL_VALIDATE_SCHEMA, remote_config())
            for _ in range(2)
        ]
        for future in futures:
            future.result()
    assert cache_file().read_bytes() == FIRMWARE
    assert downloader.call_count == 2
    assert list(cache_file().parent.iterdir()) == [cache_file()]


@pytest.mark.asyncio
@pytest.mark.parametrize("source", ["path", "url"])
async def test_codegen_embeds_verified_bytes_without_runtime_http(
    tmp_path: Path, downloader: Mock, monkeypatch: pytest.MonkeyPatch, source: str
) -> None:
    if source == "path":
        image = tmp_path / "c6.bin"
        image.write_bytes(FIRMWARE)
        fields = {"path": image}
    else:
        fields = {"url": URL}
    config = embedded_schema(fields | {"sha256": DIGEST}) | {"type": "embedded"}
    hosted_update.FINAL_VALIDATE_SCHEMA(config)
    monkeypatch.setattr(
        hosted_update.update,
        "new_update",
        AsyncMock(return_value=cg.MockObj("c6_firmware")),
    )
    monkeypatch.setattr(hosted_update.cg, "register_component", AsyncMock())

    await hosted_update.to_code(config)

    emitted_bytes = bytes(
        int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", CORE.cpp_main_section)
    )
    assert emitted_bytes == FIRMWARE + bytes.fromhex(DIGEST)
    assert f"set_firmware_size({len(FIRMWARE)})" in CORE.cpp_main_section
    assert "set_firmware_sha256" in CORE.cpp_main_section
    assert not any(
        define.name == "USE_ESP32_HOSTED_HTTP_UPDATE" for define in CORE.defines
    )
    assert downloader.call_count == (1 if source == "url" else 0)
