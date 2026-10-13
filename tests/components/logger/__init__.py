import esphome.codegen as cg
from esphome.components.logger import request_log_listener
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # logger must run its to_code during C++ test builds because it configures
    # the logging subsystem used by ESP_LOG* macros throughout component code.
    manifest.enable_codegen()

    original_to_code = manifest.to_code

    async def to_code(config: ConfigType) -> None:
        await original_to_code(config)
        # The boot log buffer is off by default; the host tests exercise it and
        # read the replay through a log listener, which nothing else registers here.
        cg.add_define("USE_LOGGER_BOOT_LOG_BUFFER")
        cg.add_define("ESPHOME_LOGGER_BOOT_LOG_BUFFER_SIZE", 512)
        request_log_listener()

    manifest.to_code = to_code
