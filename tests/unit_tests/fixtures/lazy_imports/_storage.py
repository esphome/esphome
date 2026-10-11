"""Shared storage-sidecar factory for the lazy-import fixture scripts."""

from pathlib import Path

from esphome.storage_json import StorageJSON


def make_storage() -> StorageJSON:
    """A minimal post-compile esp32 sidecar the upload/logs fast path accepts.

    build_path must be set: the fast path rejects sidecars without one.
    """
    return StorageJSON(
        storage_version=1,
        name="test",
        friendly_name="Test",
        comment=None,
        esphome_version="2026.1.0",
        src_version=1,
        address="1.2.3.4",
        web_port=None,
        target_platform="ESP32S3",
        build_path=Path("/build/test"),
        firmware_bin_path=Path("/build/test/firmware.bin"),
        loaded_integrations=set(),
        loaded_platforms=set(),
        no_mdns=False,
        framework="esp-idf",
        core_platform="esp32",
        area=None,
        framework_version="5.3.1",
    )
