from typing import Any

import pytest

from esphome.components.zigbee.const import KEY_ZIGBEE
from esphome.components.zigbee.const_esp32 import (
    CONF_ATTRIBUTE_ID,
    CONF_ATTRIBUTES,
    CONF_CLUSTERS,
    KEY_ZIGBEE_EP,
    ROLE,
)
from esphome.components.zigbee.zigbee_ep_esp32 import (
    add_clusters_to_first_ep,
    add_ep,
    create_ep,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_VALUE
from esphome.core import CORE


def _cluster(attribute_id: int, value: int) -> dict[str, Any]:
    return {
        CONF_ID: "TIME",
        ROLE: "SERVER",
        CONF_ATTRIBUTES: [{CONF_ATTRIBUTE_ID: attribute_id, CONF_VALUE: value}],
    }


def test_add_clusters_to_first_ep_reports_conflicting_attribute_id() -> None:
    add_clusters_to_first_ep([_cluster(7, 1)], merge_attributes=True)

    with pytest.raises(cv.Invalid, match=r"attribute id 7"):
        add_clusters_to_first_ep([_cluster(7, 2)], merge_attributes=True)


def test_create_ep_merges_first_endpoint_cluster_attributes() -> None:
    add_clusters_to_first_ep([_cluster(7, 1)], merge_attributes=True)
    add_clusters_to_first_ep([_cluster(8, 2)], merge_attributes=True)
    add_ep({CONF_CLUSTERS: [_cluster(9, 3)]}, 1, None)

    create_ep(router=False)

    clusters = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP][1][CONF_CLUSTERS]
    assert len(clusters) == 1
    assert clusters[0][CONF_ATTRIBUTES] == [
        {CONF_ATTRIBUTE_ID: 9, CONF_VALUE: 3},
        {CONF_ATTRIBUTE_ID: 7, CONF_VALUE: 1},
        {CONF_ATTRIBUTE_ID: 8, CONF_VALUE: 2},
    ]


def test_create_ep_reports_conflicting_first_endpoint_attribute_id() -> None:
    add_clusters_to_first_ep([_cluster(7, 1)], merge_attributes=True)
    add_ep({CONF_CLUSTERS: [_cluster(7, 2)]}, 1, None)

    with pytest.raises(cv.Invalid, match=r"attribute id 7"):
        create_ep(router=False)


def test_add_ep_reports_conflicting_attr_id() -> None:
    add_ep({CONF_CLUSTERS: [_cluster(7, 1)]}, 1, None, merge_attributes=True)

    with pytest.raises(cv.Invalid, match=r"attribute id 7"):
        add_ep({CONF_CLUSTERS: [_cluster(7, 2)]}, 1, None, merge_attributes=True)


def test_create_ep_does_not_merge_unnumbered_conflicting_attributes() -> None:
    add_ep({CONF_CLUSTERS: [_cluster(7, 1)]}, None, None, merge_attributes=True)
    add_ep({CONF_CLUSTERS: [_cluster(7, 2)]}, None, None, merge_attributes=True)

    create_ep(router=False)

    endpoints = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP]
    assert len(endpoints) == 2
    assert [
        endpoint[CONF_CLUSTERS][0][CONF_ATTRIBUTES][0][CONF_VALUE]
        for endpoint in endpoints.values()
    ] == [1, 2]


def test_create_ep_merges_unnumbered_clusters_with_coexisting_attributes() -> None:
    add_ep({CONF_CLUSTERS: [_cluster(7, 1)]}, None, None, merge_attributes=True)
    add_ep({CONF_CLUSTERS: [_cluster(8, 2)]}, None, None, merge_attributes=True)

    create_ep(router=False)

    endpoints = CORE.data[KEY_ZIGBEE][KEY_ZIGBEE_EP]
    assert len(endpoints) == 1
    clusters = next(iter(endpoints.values()))[CONF_CLUSTERS]
    assert len(clusters) == 1
    assert clusters[0][CONF_ATTRIBUTES] == [
        {CONF_ATTRIBUTE_ID: 7, CONF_VALUE: 1},
        {CONF_ATTRIBUTE_ID: 8, CONF_VALUE: 2},
    ]
