import esphome.codegen as cg
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # The ExtractRegex and ExtractJson gtests in test_extract.cpp are gated on
    # USE_STORAGE_REGEX_EXTRACT / USE_STORAGE_JSON_EXTRACT, which the component to_code only
    # sets when a config uses that step -- and _wrap_manifest suppresses that to_code for the
    # unit-test build, so the gated tests would compile out. Set both flags here (same pattern
    # the time component uses for USE_TIME_TIMEZONE) so they actually build and run. The json
    # branch pulls in ArduinoJson (automation.cpp includes json_util.h); declaring the json
    # dependency makes the test build resolve the json component, whose own override registers
    # the library, so the host build finds ArduinoJson.h. All host-side -- none of this reaches
    # the firmware memory-impact job, which measures only the base test.*.yaml configs.
    manifest.dependencies = manifest.dependencies + ["json"]

    async def to_code(config):
        cg.add_build_flag("-DUSE_STORAGE_REGEX_EXTRACT")
        cg.add_build_flag("-DUSE_STORAGE_JSON_EXTRACT")

    manifest.to_code = to_code
