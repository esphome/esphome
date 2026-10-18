from esphome.components import socket as socket_component
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        socket_component.require_tcp_listener()

    manifest.to_code = to_code_testing
    manifest.dependencies = manifest.dependencies + ["packet_interface", "socket"]
