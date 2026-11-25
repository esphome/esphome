from tests.testing_helpers import ComponentManifestOverride, add_defines_to_code


def override_manifest(manifest: ComponentManifestOverride) -> None:
    # Fan::take_change only compiles with the defines the trigger codegen emits
    add_defines_to_code(
        manifest, "USE_FAN_CHANGE_TRIGGERS", "USE_FAN_CHANGE_TRIGGERS_MULTI"
    )
