from esphome.loader import TestingComponentManifest


def override_manifest(manifest: TestingComponentManifest) -> None:
    # host must run its to_code during C++ test builds because it sets up the
    # host platform target, which is the execution environment for all unit tests.
    manifest.enable_codegen()
