import yaml
from esphome.core import DocumentRange
from esphome.helpers import add_class_to_obj


class ESPHomeDataBase:
    @property
    def esp_range(self):
        return getattr(self, "_esp_range", None)

    @property
    def content_offset(self):
        return getattr(self, "_content_offset", 0)

    def from_node(self, node):
        # pylint: disable=attribute-defined-outside-init
        self._esp_range = DocumentRange.from_marks(node.start_mark, node.end_mark)
        if isinstance(node, yaml.ScalarNode):
            if node.style is not None and node.style in "|>":
                self._content_offset = 1

    def from_database(self, database):
        # pylint: disable=attribute-defined-outside-init
        self._esp_range = database.esp_range
        self._content_offset = database.content_offset


def make_data_base(value, from_database: ESPHomeDataBase = None):
    try:
        value = add_class_to_obj(value, ESPHomeDataBase)
        if from_database is not None:
            value.from_database(from_database)
        return value
    except TypeError:
        # Adding class failed, ignore error
        return value
