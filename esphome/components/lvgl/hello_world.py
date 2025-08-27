from io import StringIO

from esphome.yaml_util import parse_yaml

CONFIG = """
- obj:
    id: hello_world_card_
    pad_all: 12
    bg_color: white
    height: 100%
    width: 100%
    scrollable: false
    layout:
        type: flex
        flex_flow: column
        flex_align_cross: center
        flex_align_main: space_between
        flex_align_track: center
        pad_column: 4
    widgets:
    - obj:
        flex_grow: 3
        outline_width: 0
        border_width: 0
        pad_all: 4
        scrollable: false
        height: size_content
        width: 100%
        layout:
            type: flex
            flex_flow: row
            flex_align_cross: center
            flex_align_track: start
            flex_align_main: space_between
        widgets:
        - button:
            checkable: true
            radius: 4
            text_font: montserrat_20
            on_click:
              lvgl.label.update:
                id: hello_world_label_
                text: "Clicked!"
            widgets:
              - label:
                  text: "Button"
        - label:
            id: hello_world_title_
            text: ESPHome
            text_font: montserrat_20
            width: 100%
            text_align: center
            on_boot:
                lvgl.widget.refresh: hello_world_title_
            hidden: !lambda |-
                return lv_obj_get_width(lv_scr_act()) < 400;
        - checkbox:
            text: Checkbox
            on_click:
              lvgl.label.update:
                id: hello_world_label_
                text: "Checked!"
    - obj:
        id: hello_world_container_
        flex_grow: 8
        pad_all: 0
        outline_width: 0
        border_width: 0
        width: 100%
        scrollable: false
        on_click:
            lvgl.spinner.update:
              id: hello_world_spinner_
              arc_color: springgreen
        layout:
            type: flex
            flex_flow: row_wrap
            flex_align_cross: center
            flex_align_track: center
            flex_align_main: space_evenly
        widgets:
            - spinner:
                id: hello_world_spinner_
                indicator:
                  arc_color: tomato
                height: 100
                width: 100
                spin_time: 2s
                arc_length: 60deg
                widgets:
                    - label:
                        id: hello_world_label_
                        text: "Hello World!"
                        align: center
            - qrcode:
                text: "https://esphome.io"
                id: hello_world_qrcode_
                size: 80
                on_boot:
                    lvgl.widget.refresh: hello_world_qrcode_
                hidden: !lambda |-
                    return lv_obj_get_width(lv_scr_act()) < 240;

    - obj:
        outline_width: 0
        border_width: 0
        flex_grow: 2
        pad_all: 8
        scrollable: false
        width: 100%
        widgets:
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
