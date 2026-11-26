import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The unit-test build has no hub entity, so size the entity lists the tests register into
    # (a hub's platforms request these through cg.slot_list at code generation).
    # One empty config entry, so to_code runs
    manifest.multi_conf = False

    async def to_code_testing(config):
        cg.add_define("TOPTRONIC_SENSOR_COUNT", 4)
        cg.add_define("TOPTRONIC_INPUT_COUNT", 4)

    manifest.to_code = to_code_testing
