from pathlib import Path

from esphome.components.logger import CONF_BOOT_LOG_BUFFER_SIZE, request_log_listener
from esphome.const import CONF_ESPHOME, CONF_INCLUDES
from esphome.core import CORE
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride

TESTS_DIR = Path(__file__).parent


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # logger must run its to_code during C++ test builds because it configures
    # the logging subsystem used by ESP_LOG* macros throughout component code.
    manifest.enable_codegen()

    original_to_code = manifest.to_code

    async def to_code(config: ConfigType) -> None:
        # Only logger's own tests exercise the boot log buffer; they read the replay
        # through a log listener, which nothing else registers in this build.
        if any(Path(p) == TESTS_DIR for p in CORE.config[CONF_ESPHOME][CONF_INCLUDES]):
            config = {**config, CONF_BOOT_LOG_BUFFER_SIZE: 512}
            request_log_listener()
        await original_to_code(config)

    manifest.to_code = to_code
