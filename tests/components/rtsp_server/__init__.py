import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # No host camera platform emits USE_CAMERA, and the real to_code needs a configured server.
    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("USE_CAMERA")
        cg.add_define("MAX_RTSP_CONNECTIONS", 1)
        cg.add_define("USE_SOCKET_IMPL_BSD_SOCKETS")
        # The sessions' audio track; capturing it needs the ESP32, so the server builds without it.
        cg.add_define("USE_RTSP_SERVER_AUDIO")

    manifest.to_code = to_code_testing
