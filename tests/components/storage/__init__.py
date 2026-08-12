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
        # No test config uses a raw_* action (those need a RawStorage device the storage-only
        # test harness has no driver for), so the USE_STORAGE_RAW_ACTIONS section of
        # automation.cpp/.h is otherwise never compiled by CI. Force it into the host build for
        # compile coverage. The worker path inside it stays #ifdef USE_STORAGE_WORKER, so it is
        # the no-worker fallback that compiles here.
        cg.add_build_flag("-DUSE_STORAGE_RAW_ACTIONS")

    manifest.to_code = to_code
