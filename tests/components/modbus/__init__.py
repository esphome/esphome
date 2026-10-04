import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The C++ tests cover the Modbus TCP framing and the forward hub. The real
    # to_code only adds USE_MODBUS_TCP for a configuration that uses them.
    async def to_code_testing(config):
        cg.add_define("USE_MODBUS_TCP")

    manifest.to_code = to_code_testing
    # A MULTI_CONF component gets an empty list here, so to_code would never run.
    manifest.multi_conf = False
