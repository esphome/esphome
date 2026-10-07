from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # ClimateIR derives from climate::Climate without declaring it as a dependency.
    manifest.dependencies = manifest.dependencies + ["climate"]
