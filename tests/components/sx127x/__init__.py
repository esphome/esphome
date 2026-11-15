"""Build the SX127x C++ tests against a software SPI bus."""

import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code(config: ConfigType) -> None:
        # The tests build a software SPI bus in C++, which no configured bus asks for
        cg.add_define("USE_SPI_SOFTWARE")

    manifest.multi_conf = False
    manifest.to_code = to_code
