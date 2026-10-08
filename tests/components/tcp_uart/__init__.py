# This file's presence makes pytest treat this directory as a package named
# "tcp_uart"; required for cpp unit testing.
from esphome.components import socket as socket_component
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        # The server-role gtest needs the listener.
        socket_component.require_tcp_listener()

    manifest.to_code = to_code_testing
