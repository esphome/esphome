import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # Register and bit lists are sized by code generation; a gtest server holds at most three of each.
        cg.add_define("MODBUS_SERVER_REGISTER_COUNT", 3)
        cg.add_define("MODBUS_SERVER_BIT_COUNT", 3)

    # A MULTI_CONF component gets no entry in the host build, so its to_code would never run.
    manifest.multi_conf = False
    manifest.to_code = to_code_testing
