from esphome.components import socket as socket_component
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        socket_component.require_tcp_listener()
        # No entity platforms are configured, so count the connected sensor to emit USE_BINARY_SENSOR
        CORE.platform_counts["binary_sensor"] += 1

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + [
        "binary_sensor",
        "packet_interface",
        "socket",
    ]
