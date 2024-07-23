from esphome import codegen as cg, config_validation as cv
from esphome.const import CONF_GROUP, CONF_ID, CONF_STATE, CONF_TYPE
from esphome.core import ID, Lambda

from .defines import (
    CONF_DEFAULT,
    CONF_FLEX_ALIGN_CROSS,
    CONF_FLEX_ALIGN_MAIN,
    CONF_FLEX_ALIGN_TRACK,
    CONF_FLEX_FLOW,
    CONF_GRID_COLUMN_ALIGN,
    CONF_GRID_COLUMNS,
    CONF_GRID_ROW_ALIGN,
    CONF_GRID_ROWS,
    CONF_LAYOUT,
    CONF_MAIN,
    CONF_SCROLLBAR_MODE,
    CONF_STYLES,
    CONF_WIDGETS,
    OBJ_FLAGS,
    PARTS,
    STATES,
    TYPE_FLEX,
    TYPE_GRID,
    join_enums,
)
from .helpers import add_lv_use, get_line_marks, join_lines
from .schemas import ALL_STYLES
from .types import WIDGET_TYPES, LValidator, LvCompound, lv_coord_t, lv_obj_t
from .widget import Widget, WidgetType, theme_widget_map


def cgen(*args):
    cg.add(cg.RawExpression("\n".join(args)))


lv_groups = set()  # Widget group names


def add_group(name):
    if name is None:
        return None
    fullname = f"lv_esp_group_{name}"
    if name not in lv_groups:
        cgen(f"static lv_group_t * {fullname} = lv_group_create()")
        lv_groups.add(name)
    return fullname


def collect_props(config):
    props = {}
    for prop in [*ALL_STYLES, *OBJ_FLAGS, CONF_STYLES, CONF_GROUP]:
        if prop in config:
            props[prop] = config[prop]
    return props


def collect_states(config):
    states = {CONF_DEFAULT: collect_props(config)}
    for state in STATES:
        if state in config:
            states[state] = collect_props(config[state])
    return states


def collect_parts(config):
    parts = {CONF_MAIN: collect_states(config)}
    for part in PARTS:
        if part in config:
            parts[part] = collect_states(config[part])
    return parts


async def set_obj_properties(w: Widget, config):
    """Return a list of C++ statements to apply properties to an lv_obj_t"""
    init = []
    if layout := config.get(CONF_LAYOUT):
        layout_type: str = layout[CONF_TYPE]
        init.extend(
            w.set_property(CONF_LAYOUT, f"LV_LAYOUT_{layout_type.upper()}", ltype="obj")
        )
        if layout_type == TYPE_GRID:
            wid = config[CONF_ID]
            rows = "{" + ",".join(layout[CONF_GRID_ROWS]) + ", LV_GRID_TEMPLATE_LAST}"
            row_id = ID(f"{wid}_row_dsc", is_declaration=True, type=lv_coord_t)
            row_array = cg.static_const_array(row_id, cg.RawExpression(rows))
            init.extend(w.set_style("grid_row_dsc_array", row_array, 0))
            columns = (
                "{" + ",".join(layout[CONF_GRID_COLUMNS]) + ", LV_GRID_TEMPLATE_LAST}"
            )
            column_id = ID(f"{wid}_column_dsc", is_declaration=True, type=lv_coord_t)
            column_array = cg.static_const_array(column_id, cg.RawExpression(columns))
            init.extend(w.set_style("grid_column_dsc_array", column_array, 0))
            init.extend(
                w.set_style(
                    CONF_GRID_COLUMN_ALIGN, layout.get(CONF_GRID_COLUMN_ALIGN), 0
                )
            )
            init.extend(
                w.set_style(CONF_GRID_ROW_ALIGN, layout.get(CONF_GRID_ROW_ALIGN), 0)
            )
        if layout_type == TYPE_FLEX:
            init.extend(w.set_property(CONF_FLEX_FLOW, layout, ltype="obj"))
            main = layout[CONF_FLEX_ALIGN_MAIN]
            cross = layout[CONF_FLEX_ALIGN_CROSS]
            track = layout[CONF_FLEX_ALIGN_TRACK]
            init.append(f"lv_obj_set_flex_align({w.obj}, {main}, {cross}, {track})")
    parts = collect_parts(config)
    for part, states in parts.items():
        for state, props in states.items():
            lv_state = f"(int)LV_STATE_{state.upper()}|(int)LV_PART_{part.upper()}"
            if styles := props.get(CONF_STYLES):
                for style_id in styles:
                    init.append(f"lv_obj_add_style({w.obj}, {style_id}, {lv_state})")
            for prop, value in {
                k: v for k, v in props.items() if k in ALL_STYLES
            }.items():
                if isinstance(ALL_STYLES[prop], LValidator):
                    value = await ALL_STYLES[prop].process(value)
                init.extend(w.set_style(prop, value, lv_state))
    if group := add_group(config.get(CONF_GROUP)):
        init.append(f"lv_group_add_obj({group}, {w.obj})")
    flag_clr = set()
    flag_set = set()
    props = parts[CONF_MAIN][CONF_DEFAULT]
    for prop, value in {k: v for k, v in props.items() if k in OBJ_FLAGS}.items():
        if value:
            flag_set.add(prop)
        else:
            flag_clr.add(prop)
    if flag_set:
        adds = join_enums(flag_set, "LV_OBJ_FLAG_")
        init.extend(w.add_flag(adds))
    if flag_clr:
        clrs = join_enums(flag_clr, "LV_OBJ_FLAG_")
        init.extend(w.clear_flag(clrs))

    if states := config.get(CONF_STATE):
        adds = set()
        clears = set()
        lambs = {}
        for key, value in states.items():
            if isinstance(value, cv.Lambda):
                lambs[key] = value
            elif value == "true":
                adds.add(key)
            else:
                clears.add(key)
        if adds:
            adds = join_enums(adds, "LV_STATE_")
            init.extend(w.add_state(adds))
        if clears:
            clears = join_enums(clears, "LV_STATE_")
            init.extend(w.clear_state(clears))
        for key, value in lambs.items():
            lamb = await cg.process_lambda(value, [], return_type=cg.bool_)
            init.append(
                f"""
                if({lamb}())
                    lv_obj_add_state({w.obj}, LV_STATE_{key.upper()});
                else
                    lv_obj_clear_state({w.obj}, LV_STATE_{key.upper()});
                """
            )
    if scrollbar_mode := config.get(CONF_SCROLLBAR_MODE):
        init.append(f"lv_obj_set_scrollbar_mode({w.obj}, {scrollbar_mode})")
    return init


async def action_to_code(action, action_id, w: Widget, template_arg, args):
    if nc := w.check_null():
        action.insert(0, nc)
    lamb = await cg.process_lambda(Lambda(join_lines(action, action_id)), args)
    var = cg.new_Pvariable(action_id, template_arg, lamb)
    return var


async def update_to_code(config, action_id, widget: Widget, init, template_arg, args):
    if config is not None:
        init.extend(await set_obj_properties(widget, config))
        if (
            widget.type.w_type.value_property is not None
            and widget.type.w_type.value_property in config
        ):
            init.append(
                f" lv_event_send({widget.obj}, LV_EVENT_VALUE_CHANGED, nullptr)"
            )
    return await action_to_code(init, action_id, widget, template_arg, args)


async def widget_to_code(w_cnfig, w_type, parent):
    init = []
    spec: WidgetType = WIDGET_TYPES.get(w_type)
    if not spec:
        raise cv.Invalid(f"No handler for widget {w_type}")
    creator = spec.obj_creator(parent, w_cnfig)
    add_lv_use(spec.name)
    add_lv_use(*spec.get_uses())
    wid = w_cnfig[CONF_ID]
    init.extend(get_line_marks(wid))

    if wid.type.inherits_from(LvCompound):
        var = cg.new_Pvariable(wid)
        init.append(f"{var}->set_obj({creator})")
        obj = f"{var}->obj"
    else:
        var = cg.Pvariable(wid, cg.nullptr, type_=lv_obj_t)
        init.append(f"{var} = {creator}")
        obj = var

    widget = Widget.create(wid, var, spec, w_cnfig, obj, parent)
    if theme := theme_widget_map.get(w_type):
        init.append(f"{theme}({obj})")
    init.extend(await set_obj_properties(widget, w_cnfig))
    if widgets := w_cnfig.get(CONF_WIDGETS):
        for w in widgets:
            sub_type, sub_config = next(iter(w.items()))
            init.extend(await widget_to_code(sub_config, sub_type, widget.obj))
    init.extend(await spec.to_code(widget, w_cnfig))
    return init


async def add_widgets(parent: Widget, config: dict):
    init = []
    if widgets := config.get(CONF_WIDGETS):
        for w in widgets:
            w_type, w_cnfig = next(iter(w.items()))
            ext_init = await widget_to_code(w_cnfig, w_type, parent.obj)
            init.extend(ext_init)
    return init
