import esphome.codegen as cg
from esphome.core import CORE
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # USE_API compiles every api source, so emit what they need. No socket
    # override: an __init__.py there makes pytest import its conftest as socket.conftest.
    async def to_code_testing(config):
        cg.add_define("USE_API")
        cg.add_define("USE_API_PLAINTEXT")
        cg.add_define("API_MAX_SEND_QUEUE", 8)
        cg.add_define("MAX_API_CONNECTIONS", 1)
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")
        CORE.register_controller()  # api_server registers with the controller registry

    manifest.to_code = to_code_testing
