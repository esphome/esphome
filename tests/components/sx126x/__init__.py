"""Enable real logger callbacks for radio reentrancy regression tests."""

import esphome.codegen as cg
from esphome.components.logger import request_log_listener
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config: ConfigType) -> None:
        request_log_listener()
        # The tests build a software SPI bus in C++, which no configured bus asks for
        cg.add_define("USE_SPI_SOFTWARE")

    manifest.multi_conf = False
    manifest.to_code = to_code
