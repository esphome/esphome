from io import StringIO

from esphome.yaml_util import parse_yaml

CONFIG = """
- obj:
    radius: 0
    pad_all: 12
    bg_color: 0xFFFFFF
    height: 100%
    width: 100%
    widgets:
    - spinner:
        id: hello_world_spinner_
        align: center
        indicator:
          arc_color: tomato
        height: 100
        width: 100
        spin_time: 2s
        arc_length: 60deg
    - label:
        id: hello_world_label_
        text: "Hello World!"
        align: center
        on_click:
            lvgl.spinner.update:
              id: hello_world_spinner_
              arc_color: springgreen
    - checkbox:
        pad_all: 8
        text: Checkbox
        align: top_right
        on_click:
          lvgl.label.update:
            id: hello_world_label_
            text: "Checked!"
    - button:
        pad_all: 8
        checkable: true
        align: top_left
        text_font: montserrat_20
        on_click:
          lvgl.label.update:
            id: hello_world_label_
            text: "Clicked!"
        widgets:
          - label:
              text: "Button"
    - slider:
        width: 80%
        align: bottom_mid
        on_value:
          lvgl.label.update:
            id: hello_world_label_
            text:
              format: "%.0f%%"
              args: [x]
"""


def get_hello_world():
    with StringIO(CONFIG) as fp:
        return parse_yaml("hello_world", fp)
