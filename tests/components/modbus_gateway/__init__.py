import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # One config entry, so the stub runs once.
    manifest.multi_conf = False

    async def to_code_testing(config: ConfigType) -> None:
        # The port buffers are sized by code generation; the gtests use up to four ports.
        cg.add_define("MODBUS_GATEWAY_PORT_COUNT", 4)

    manifest.to_code = to_code_testing
