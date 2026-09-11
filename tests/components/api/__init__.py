import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # api and socket to_code are suppressed in unit test builds; USE_API compiles
    # every api source, so emit what they need. No socket override: an
    # __init__.py there makes pytest import its conftest as socket.conftest.
    async def to_code_testing(config):
        cg.add_define("USE_API")
        cg.add_define("USE_API_PLAINTEXT")
        cg.add_define("API_MAX_SEND_QUEUE", 8)
        cg.add_define("MAX_API_CONNECTIONS", 1)
        cg.add_define("USE_CONTROLLER_REGISTRY")
        cg.add_define("CONTROLLER_REGISTRY_MAX", 1)
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")

    manifest.to_code = to_code_testing
