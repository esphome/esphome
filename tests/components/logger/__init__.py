from esphome.loader import TestingComponentManifest


def override_manifest(manifest: TestingComponentManifest) -> None:
    # logger must run its to_code during C++ test builds because it configures
    # the logging subsystem used by ESP_LOG* macros throughout component code.
    manifest.enable_codegen()
