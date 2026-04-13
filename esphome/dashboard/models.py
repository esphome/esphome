"""Data models and builders for the dashboard."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import TYPE_CHECKING, TypedDict

if TYPE_CHECKING:
    from esphome.zeroconf import DiscoveredImport

    from .core import ESPHomeDashboard
    from .entries import DashboardEntry

_LOGGER = logging.getLogger(__name__)


class ImportableDeviceDict(TypedDict):
    """Dictionary representation of an importable device."""

    name: str
    friendly_name: str | None
    package_import_url: str
    project_name: str
    project_version: str
    network: str
    ignored: bool


class ConfiguredDeviceDict(TypedDict, total=False):
    """Dictionary representation of a configured device."""

    name: str
    friendly_name: str | None
    configuration: str
    loaded_integrations: list[str] | None
    deployed_version: str | None
    current_version: str | None
    path: str
    comment: str | None
    address: str | None
    web_port: int | None
    target_platform: str | None
    tags: list[str]


class ArchivedDeviceDict(TypedDict, total=False):
    """Dictionary representation of an archived device."""

    name: str
    friendly_name: str | None
    configuration: str
    comment: str | None
    address: str | None
    target_platform: str | None
    tags: list[str]


class DeviceListResponse(TypedDict):
    """Response for device list API."""

    configured: list[ConfiguredDeviceDict]
    importable: list[ImportableDeviceDict]
    archived: list[ArchivedDeviceDict]


def build_importable_device_dict(
    dashboard: ESPHomeDashboard, discovered: DiscoveredImport
) -> ImportableDeviceDict:
    """Build the importable device dictionary."""
    return ImportableDeviceDict(
        name=discovered.device_name,
        friendly_name=discovered.friendly_name,
        package_import_url=discovered.package_import_url,
        project_name=discovered.project_name,
        project_version=discovered.project_version,
        network=discovered.network,
        ignored=discovered.device_name in dashboard.ignored_devices,
    )


def build_archived_device_list(
    tags: dict[str, list[str]] | None = None,
) -> list[ArchivedDeviceDict]:
    """Scan the archive directory and build a list of archived devices."""
    from esphome.storage_json import StorageJSON, archive_storage_path, ext_storage_path

    if tags is None:
        tags = {}

    try:
        archive_path = archive_storage_path()
        if not archive_path.is_dir():
            return []
    except Exception:  # pylint: disable=broad-except
        return []

    archived: list[ArchivedDeviceDict] = []
    for path in sorted(archive_path.iterdir()):
        if path.suffix not in (".yaml", ".yml"):
            continue
        filename = path.name
        storage = StorageJSON.load(ext_storage_path(filename))
        if storage is not None:
            archived.append(
                ArchivedDeviceDict(
                    name=storage.name,
                    friendly_name=storage.friendly_name,
                    configuration=filename,
                    comment=storage.comment,
                    address=storage.address,
                    target_platform=storage.target_platform,
                    tags=tags.get(filename, []),
                )
            )
        else:
            name = Path(filename).stem.replace("-", " ").replace("_", " ")
            archived.append(
                ArchivedDeviceDict(
                    name=name,
                    friendly_name=None,
                    configuration=filename,
                    comment=None,
                    address=None,
                    target_platform=None,
                    tags=tags.get(filename, []),
                )
            )
    return archived


def build_device_list_response(
    dashboard: ESPHomeDashboard, entries: list[DashboardEntry]
) -> DeviceListResponse:
    """Build the device list response data."""
    configured_names = {entry.name for entry in entries}
    try:
        tags = dashboard.device_tags
    except Exception:  # pylint: disable=broad-except
        tags = {}
    configured = []
    for entry in entries:
        d = dict(entry.to_dict())
        d["tags"] = tags.get(entry.filename, [])
        configured.append(d)
    try:
        archived = build_archived_device_list(tags)
    except Exception:  # pylint: disable=broad-except
        _LOGGER.exception("Failed to build archived device list")
        archived = []
    return DeviceListResponse(
        configured=configured,
        importable=[
            build_importable_device_dict(dashboard, res)
            for res in dashboard.import_result.values()
            if res.device_name not in configured_names
        ],
        archived=archived,
    )
