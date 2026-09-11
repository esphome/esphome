import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The api and socket to_code are suppressed in unit test builds. USE_API
    # compiles every api source, so emit what they need: a frame helper, the
    # connection limits, the controller registry that api_server registers
    # with (core only emits it from the real api to_code), and the host socket.
    async def to_code_testing(config):
        cg.add_define("USE_API")
        cg.add_define("USE_API_PLAINTEXT")
        cg.add_define("API_MAX_SEND_QUEUE", 8)
        cg.add_define("MAX_API_CONNECTIONS", 1)
        cg.add_define("USE_CONTROLLER_REGISTRY")
        cg.add_define("CONTROLLER_REGISTRY_MAX", 1)
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")

    manifest.to_code = to_code_testing
