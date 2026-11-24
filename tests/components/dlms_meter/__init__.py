import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The real to_code runs for the instance in benchmark.yaml: it adds the dlms_parser library
    # and, on the host, the system mbedtls
    manifest.enable_codegen()
    real_to_code = manifest.to_code

    async def to_code(config: ConfigType) -> None:
        await real_to_code(config)
        # Sensor storage is sized by code generation; the test registers one sensor
        cg.add_define("DLMS_MAX_SENSORS", 1)

    manifest.to_code = to_code
