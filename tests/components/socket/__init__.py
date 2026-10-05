# This file's presence makes pytest treat this directory as a package named "socket",
# shadowing the stdlib module -- no test_*.py (Python-level) tests can live here.
# required for cpp unit testing.
import esphome.codegen as cg
from esphome.components import network, socket as socket_component
from esphome.core import CORE
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.enable_codegen()
    real_to_code = manifest.to_code

    async def to_code_testing(config):
        await real_to_code(config)
        cg.add_define("USE_NETWORK_IPV6", True)
        # The gtests link against the filtered helper files, which need IPv4.
        if not CORE.data.get(network.KEY_REQUEST_IPV4_OFF):
            socket_component.require_tcp_client_link()

    manifest.to_code = to_code_testing
