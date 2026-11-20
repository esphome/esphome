"""Keep compact endpoint slots separate from application endpoint IDs."""

from dataclasses import dataclass, field

import esphome.config_validation as cv
from esphome.core import CORE

from .const import CONF_MAX_EP_NUMBER, KEY_ZIGBEE

KEY_ENDPOINT_IDS = "endpoint_ids"


@dataclass
class EndpointIds:
    reserved: set[int] = field(default_factory=set)
    assigned: dict[int, int] = field(default_factory=dict)

    def reserve(self, endpoint: int) -> None:
        if endpoint in self.reserved:
            raise cv.Invalid(f"Duplicate Zigbee endpoint: {endpoint}")
        self.reserved.add(endpoint)

    def resolve(self, slot: int, endpoint: int | None = None) -> int:
        if slot in self.assigned:
            if endpoint is not None and endpoint != self.assigned[slot]:
                raise cv.Invalid(f"Zigbee endpoint slot {slot} is already assigned")
            return self.assigned[slot]
        if endpoint is not None and endpoint not in self.reserved:
            raise cv.Invalid(f"Zigbee endpoint {endpoint} is not reserved")
        if endpoint is None:
            used = self.reserved | set(self.assigned.values())
            endpoint = next(
                i for i in range(1, CONF_MAX_EP_NUMBER + 1) if i not in used
            )
        elif endpoint in self.assigned.values():
            raise cv.Invalid(f"Zigbee endpoint {endpoint} is already assigned")
        self.assigned[slot] = endpoint
        return endpoint


def endpoint_ids() -> EndpointIds:
    data = CORE.data.setdefault(KEY_ZIGBEE, {})
    if KEY_ENDPOINT_IDS not in data:
        data[KEY_ENDPOINT_IDS] = EndpointIds()
    return data[KEY_ENDPOINT_IDS]
