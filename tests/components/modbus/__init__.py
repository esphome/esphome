import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    async def to_code_testing(config: ConfigType) -> None:
        # The on_request callback storage is sized by code generation; the gtests add up to two per hub.
        cg.add_define("MODBUS_ON_REQUEST_COUNT", 2)
        cg.add_define("MODBUS_ON_RESPONSE_COUNT", 1)

    # A MULTI_CONF component gets no entry in the host build, so its to_code would never run.
    manifest.multi_conf = False
    manifest.to_code = to_code_testing
