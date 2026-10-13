# This file's presence makes pytest treat this directory as a package named
# "vnc"; required for cpp unit testing.
from esphome.components import socket as socket_component
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # MULTI_CONF components get no config in a unit test build, so to_code would never run.
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        socket_component.require_tcp_client_link()

    manifest.to_code = to_code_testing
