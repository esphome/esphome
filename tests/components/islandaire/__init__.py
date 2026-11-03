from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # AUTO_LOAD = ["climate_ir"] lives on the climate platform (climate.py), and climate_ir itself does
    # not declare climate even though ClimateIR derives from climate::Climate. The unit test build
    # resolves the bare component, so pull both in for the header to compile.
    manifest.dependencies = manifest.dependencies + ["climate_ir", "climate"]
