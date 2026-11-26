from esphome.components.logger import request_log_listener
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # benchmark.yaml configures a udp packet_interface, whose to_code waits on the udp component variable.
    manifest.enable_codegen()
    original_to_code = manifest.to_code

    # The tests check warnings through a real logger callback
    async def to_code(config: ConfigType) -> None:
        await original_to_code(config)
        request_log_listener()

    if hasattr(original_to_code, "priority"):
        to_code.priority = original_to_code.priority
    manifest.to_code = to_code
