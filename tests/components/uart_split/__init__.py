# This file's presence makes pytest treat this directory as a package named
# "uart_split"; required for cpp unit testing.
import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.multi_conf = False

    # Only the output count define; the gtests build their outputs by hand.
    async def to_code(config: ConfigType) -> None:
        cg.add_define("UART_SPLIT_OUTPUT_COUNT", 3)

    manifest.to_code = to_code
