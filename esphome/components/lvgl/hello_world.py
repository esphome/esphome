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
    widgets:
    - obj:
        align: top_mid
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
            id: hello_world_checkbox_
            on_boot:
                lvgl.widget.refresh: hello_world_checkbox_
            hidden: !lambda |-
                return lv_obj_get_width(lv_scr_act()) < 240;
            on_click:
              lvgl.label.update:
                id: hello_world_label_
                text: "Checked!"
    - obj:
        id: hello_world_container_
        align: center
        y: 14
        pad_all: 0
        outline_width: 0
        border_width: 0
        width: 100%
        height: size_content
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
            - obj:
                id: hello_world_qrcode_
                outline_width: 0
                border_width: 0
                hidden: !lambda |-
                    return lv_obj_get_width(lv_scr_act()) < 300 && lv_obj_get_height(lv_scr_act()) < 400;
                widgets:
                - label:
                    text_font: montserrat_14
                    text: esphome.io
                    align: top_mid
                - qrcode:
                    text: "https://esphome.io"
                    size: 80
                    align: bottom_mid
                    on_boot:
                        lvgl.widget.refresh: hello_world_qrcode_

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
