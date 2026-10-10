from esphome.components import uart
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
DOMAIN = "bridge"

IS_PLATFORM_COMPONENT = True


def claim_exclusive(config: ConfigType, conf_key: str, label: str) -> None:
    """Reject the config unless this bridge is the only user of the interface at conf_key.

    Bridges of any platform must own their interfaces exclusively; shared ring buffers and
    overwritten callbacks would corrupt both streams silently. All platforms and keys share
    one seen-set, because every bridged interface is a uart::UARTComponent (a CDC-ACM
    instance too) and ids are unique. Other components bind an interface through a uart_id
    key. Bare `id:` references (a uart.write action) cannot be distinguished; not caught.
    """
    full_config = fv.full_config.get()
    owned_id = str(config[conf_key])
    used = full_config.data.setdefault(DOMAIN, set())
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
        if uart.subtree_references_uart(domain_conf, owned_id):
            raise cv.Invalid(
                f"The {label} '{owned_id}' is also used by '{domain}'; a bridge "
                f"requires exclusive use of its {label}.",
                [conf_key],
            )
