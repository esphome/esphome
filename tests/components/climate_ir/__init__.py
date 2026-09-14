from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # ClimateIR derives from climate::Climate but doesn't declare `climate` as a
    # dependency (it's loaded as the `climate:` config domain in normal builds).
    # The unit test instantiates ClimateIR directly, so pull climate in explicitly.
    manifest.dependencies = manifest.dependencies + ["climate"]
