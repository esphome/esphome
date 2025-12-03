"""Data models and builders for the dashboard."""

from __future__ import annotations

from typing import TYPE_CHECKING, TypedDict

if TYPE_CHECKING:
    from esphome.zeroconf import DiscoveredImport

    from .core import ESPHomeDashboard
    from .entries import DashboardEntry


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


class DeviceListResponse(TypedDict):
    """Response for device list API."""

    configured: list[ConfiguredDeviceDict]
    importable: list[ImportableDeviceDict]


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


def build_device_list_response(
    dashboard: ESPHomeDashboard, entries: list[DashboardEntry]
) -> DeviceListResponse:
    """Build the device list response data."""
    configured = {entry.name for entry in entries}
    return DeviceListResponse(
        configured=[entry.to_dict() for entry in entries],
        importable=[
            build_importable_device_dict(dashboard, res)
            for res in dashboard.import_result.values()
            if res.device_name not in configured
        ],
    )
