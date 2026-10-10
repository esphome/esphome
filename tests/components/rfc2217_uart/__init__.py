# This file's presence makes pytest treat this directory as a package named
# "rfc2217_uart"; required for cpp unit testing.
import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        # The gtests link the client, which only a client config compiles in.
        cg.add_define("USE_RFC2217_UART_CLIENT")

    manifest.to_code = to_code_testing
