from esphome.loader import TestingComponentManifest


def override_manifest(manifest: TestingComponentManifest) -> None:
    # core (esphome/core/config.py) must run its to_code during C++ test builds
    # because it bootstraps the fundamental application infrastructure that all
    # components depend on (component registration, event loop, etc.).
    manifest.enable_codegen()
