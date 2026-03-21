import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:

    # This function is executed instead of to_code() during c++ testing
    async def to_code_testing(config):
        # During testing, enable decryption code unconditionally.
        # USE_BTHOME_DECRYPTION also enables bthome_encrypt (for use in tests only).
        cg.add_define("USE_BTHOME_DECRYPTION")

        # During testing, enable all sensor types unconditionally.
        # The entity count defines are normally generated from CORE.platform_counts by
        # _add_platform_defines(). Sensor and binary_sensor counts are set via their
        # platform components (sensor.bthome / binary_sensor.bthome). Text sensor has
        # no bthome platform component, so we set its count explicitly here.
        cg.add_define("USE_SENSOR")
        cg.add_define("USE_BINARY_SENSOR")
        cg.add_define("USE_TEXT_SENSOR")
        cg.add_define("ESPHOME_ENTITY_TEXT_SENSOR_COUNT", 1)

        # Pull mbedtls for testing in host environment
        cg.add_library("baracodadailyhealthtech/mbedtls", "3.6.1-1", None)

    manifest.to_code = to_code_testing
    manifest.auto_load = manifest.auto_load + ["text_sensor", "binary_sensor", "sensor"]
