"""nRF52 endpoint IDs: configured ones are reserved during validation, the rest allocated at codegen."""

from dataclasses import dataclass, field

import esphome.config_validation as cv
from esphome.core import CORE

from .const import CONF_MAX_EP_NUMBER, KEY_ZIGBEE

KEY_ENDPOINT_IDS = "endpoint_ids"


@dataclass
class EndpointIds:
    reserved: set[int] = field(default_factory=set)

    def reserve(self, endpoint: int) -> None:
        if endpoint in self.reserved:
            raise cv.Invalid(f"Duplicate Zigbee endpoint: {endpoint}")
        self.reserved.add(endpoint)

    def allocate(self) -> int:
        """Take the lowest free endpoint ID."""
        endpoint = next(
            i for i in range(1, CONF_MAX_EP_NUMBER + 1) if i not in self.reserved
        )
        self.reserved.add(endpoint)
        return endpoint


def endpoint_ids() -> EndpointIds:
    return CORE.data.setdefault(KEY_ZIGBEE, {}).setdefault(
        KEY_ENDPOINT_IDS, EndpointIds()
    )
