import esphome.config_validation as cv
from esphome.const import CONF_UART_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DOMAIN = "bridge"

IS_PLATFORM_COMPONENT = True


def _subtree_references_uart(node: object, uart_id: str) -> bool:
    """Return True if any dict in the subtree has a uart_id entry naming this bus."""
    if isinstance(node, dict):
        return any(
            (key == CONF_UART_ID and str(value) == uart_id)
            or _subtree_references_uart(value, uart_id)
            for key, value in node.items()
        )
    if isinstance(node, list):
        return any(_subtree_references_uart(item, uart_id) for item in node)
    return False


def claim_exclusive(
    config: ConfigType, conf_key: str, label: str, *, seen_key: str | None = None
) -> None:
    """Reject the config unless this bridge is the only user of the interface at conf_key.

    Bridges of any platform must own their interfaces exclusively; shared ring buffers and
    overwritten callbacks would corrupt both streams silently. The seen-set is keyed on the
    bridge domain so all platforms share it; seen_key (default conf_key) names the set, so
    keys that hold the same kind of interface share one. Other components bind an interface
    through a uart_id key. Bare `id:` references (a uart.write action) cannot be
    distinguished; not caught.
    """
    full_config = fv.full_config.get()
    owned_id = str(config[conf_key])
    data = full_config.data.setdefault(DOMAIN, {})
    used = data.setdefault(seen_key or conf_key, set())
    if owned_id in used:
        raise cv.Invalid(
            f"The {label} '{owned_id}' is already bridged by another 'bridge' "
            f"instance; each bridge requires its own {label}.",
            [conf_key],
        )
    used.add(owned_id)
    for domain, domain_conf in full_config.items():
        if domain == DOMAIN:
            continue
        if _subtree_references_uart(domain_conf, owned_id):
            raise cv.Invalid(
                f"The {label} '{owned_id}' is also used by '{domain}'; a bridge "
                f"requires exclusive use of its {label}.",
                [conf_key],
            )
