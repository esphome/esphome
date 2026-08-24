from esphome import automation
from esphome.automation import Trigger
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_BUTTON, CONF_ID, CONF_PAGES, CONF_TRIGGER_ID

from ..automation import action_to_code
from ..defines import (
    CONF_HEADER_MODE,
    CONF_HEADER_STYLE,
    CONF_IMAGE,
    CONF_MAIN,
    CONF_MENU,
    CONF_ON_ROOT_BACK_BUTTON_CLICK,
    CONF_PAGE,
    CONF_ROOT_BACK_BTN,
    CONF_ROOT_PAGE,
    CONF_SECTIONS,
    CONF_SIDEBAR_PAGE,
    CONF_SIDEBAR_STYLE,
    CONF_TITLE,
    LV_MENU_MODES,
    LV_MENU_ROOT_BACK_BUTTON_MODES,
    TYPE_FLEX,
    get_menu_set_page_actions,
    get_widget_map,
)
from ..lv_validation import lv_text
from ..lvcode import (
    EVENT_ARG,
    LambdaContext,
    LocalVariable,
    LvConditional,
    lv,
    lv_add,
    lv_expr,
    lv_obj,
)
from ..schemas import container_schema, part_schema
from ..types import LV_EVENT, LvType, ObjUpdateAction, lv_obj_t, lv_obj_t_ptr
from . import (
    Widget,
    WidgetType,
    get_widget_,
    get_widgets,
    set_obj_properties,
    widget_to_code,
)
from .obj import obj_spec

# Local to the "menu" widget only -- not a shared config concept.
CONF_SIDEBAR = "sidebar"

# Tag types, only used to distinguish declare_id()/use_id() checks for pages and
# sections -- like tileview's lv_tileview_tile_t they carry no extra behaviour, but
# they must have their own names: id type checks compare the type name, so reusing
# `lv_obj_t` here would let any object be passed where a page is required. The
# generated code always declares the variable as `lv_obj_t *`.
lv_menu_page_t = LvType("lv_menu_page_t")
lv_menu_section_t = LvType("lv_menu_section_t")

lv_menu_t = LvType(
    "lv_menu_t",
    largs=[(lv_obj_t_ptr, "page")],
    lvalue=lambda w: lv_expr.menu_get_cur_main_page(w.obj),
    has_on_value=True,
)


class MenuSectionType(WidgetType):
    """
    A grouped, bordered set of rows inside a menu page (lv_menu_section_create).
    Only valid as a direct child of a menu page, so it's not exposed in the
    generic `widgets:` menu -- reached only via a page's `sections:` key.
    """

    def __init__(self):
        super().__init__(
            "menu_section", lv_menu_section_t, (CONF_MAIN,), {}, is_mock=True
        )

    async def obj_creator(self, parent: cg.MockObj, config: dict):
        return lv_expr.menu_section_create(parent)


menu_section_spec = MenuSectionType()

MENU_PAGE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_TITLE): lv_text,
        cv.Optional(CONF_SECTIONS): cv.ensure_list(
            container_schema(
                menu_section_spec,
                {cv.GenerateID(): cv.declare_id(lv_menu_section_t)},
            )
        ),
    }
)


class MenuPageType(WidgetType):
    """
    A page of the menu (lv_menu_page_create), shown one at a time via
    `lvgl.menu.set_page`. Like tabview's tabs/tileview's tiles, pages are only
    ever declared via the menu's own `pages:` key, not the generic `widgets:`
    menu -- reachable content, not a freestanding widget type.
    """

    def __init__(self):
        super().__init__(
            "menu_page", lv_menu_page_t, (CONF_MAIN,), MENU_PAGE_SCHEMA, is_mock=True
        )

    async def obj_creator(self, parent: cg.MockObj, config: dict):
        title = config.get(CONF_TITLE)
        return lv_expr.menu_page_create(
            parent, await lv_text.process(title) if title is not None else cg.nullptr
        )

    async def to_code(self, w: Widget, config: dict):
        for section_conf in config.get(CONF_SECTIONS, ()):
            await widget_to_code(section_conf, menu_section_spec, w.obj)


menu_page_spec = MenuPageType()

MENU_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PAGES): cv.ensure_list(
            container_schema(
                menu_page_spec, {cv.GenerateID(): cv.declare_id(lv_menu_page_t)}
            )
        ),
        cv.Required(CONF_ROOT_PAGE): cv.use_id(lv_menu_page_t),
        cv.Optional(CONF_SIDEBAR_PAGE): cv.use_id(lv_menu_page_t),
        cv.Optional(CONF_HEADER_MODE, default="top_fixed"): LV_MENU_MODES.one_of,
        cv.Optional(
            CONF_ROOT_BACK_BTN, default="disabled"
        ): LV_MENU_ROOT_BACK_BUTTON_MODES.one_of,
        cv.Optional(CONF_HEADER_STYLE): part_schema(obj_spec.parts),
        cv.Optional(CONF_SIDEBAR_STYLE): part_schema(obj_spec.parts),
        cv.Optional(CONF_ON_ROOT_BACK_BUTTON_CLICK): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(Trigger.template())}
        ),
    }
)


class MenuType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_MENU, lv_menu_t, (CONF_MAIN,), schema=MENU_SCHEMA, modify_schema={}
        )

    def get_uses(self):
        return CONF_BUTTON, CONF_IMAGE, TYPE_FLEX

    async def to_code(self, w: Widget, config: dict):
        await w.set_property(
            "mode_header", await LV_MENU_MODES.process(config[CONF_HEADER_MODE])
        )
        await w.set_property(
            "mode_root_back_button",
            await LV_MENU_ROOT_BACK_BUTTON_MODES.process(config[CONF_ROOT_BACK_BTN]),
        )
        for page_conf in config[CONF_PAGES]:
            await widget_to_code(page_conf, menu_page_spec, w.obj)

        root_page = await get_widget_(config[CONF_ROOT_PAGE])
        lv.menu_set_page(w.obj, root_page.obj)
        if sidebar_id := config.get(CONF_SIDEBAR_PAGE):
            sidebar_page = await get_widget_(sidebar_id)
            lv.menu_set_sidebar_page(w.obj, sidebar_page.obj)

        if header_style := config.get(CONF_HEADER_STYLE):
            with LocalVariable(
                "menu_header", lv_obj_t, rhs=lv_expr.menu_get_main_header(w.obj)
            ) as hdr:
                await set_obj_properties(Widget(hdr, obj_spec), header_style)
        if sidebar_style := config.get(CONF_SIDEBAR_STYLE):
            with LocalVariable(
                "menu_sidebar_header",
                lv_obj_t,
                rhs=lv_expr.menu_get_sidebar_header(w.obj),
            ) as hdr:
                await set_obj_properties(Widget(hdr, obj_spec), sidebar_style)


menu_spec = MenuType()


def _menu_page_owners(global_config) -> dict[str, tuple]:
    """
    Map every declared menu page id to the config path of the menu that owns it.

    A page id is always declared inside its menu's `pages:` list, so the owning menu
    is three steps up the path (`id` -> list index -> `pages`).
    """
    return {
        str(declared_id): tuple(path[:-3])
        for declared_id, path in global_config.declare_ids
        if declared_id.type is lv_menu_page_t
    }


def validate_menu_pages(global_config) -> None:
    """
    Check that a menu only ever refers to pages of its own.

    `lv_menu_page_create()` makes a page a child of one specific menu, so handing a
    page of another menu to `root_page:`, `sidebar_page:` or `lvgl.menu.set_page`
    can only misbehave at run time. It also hangs code generation when the other
    menu is defined later, since the waiting menu's page never appears.
    """
    owners = _menu_page_owners(global_config)
    for menu_path in set(owners.values()):
        menu_config = global_config.get_config_for_path(list(menu_path))
        for key in (CONF_ROOT_PAGE, CONF_SIDEBAR_PAGE):
            page_id = menu_config.get(key)
            if page_id is not None and owners.get(str(page_id)) != menu_path:
                raise cv.Invalid(
                    f"'{page_id}' is not a page of this menu; "
                    f"'{key}' must name one of the menu's own 'pages:'",
                    # Paths reported from final validation are relative to the
                    # component config, so drop the leading domain name.
                    path=[*menu_path[1:], key],
                )
    for config in get_menu_set_page_actions():
        menu_id = config[CONF_ID]
        page_id = config[CONF_PAGE]
        menu_path = tuple(global_config.get_path_for_id(menu_id)[:-1])
        if owners.get(str(page_id)) != menu_path:
            raise cv.Invalid(
                f"'lvgl.menu.set_page' page '{page_id}' is not a page of menu '{menu_id}'"
            )


async def add_root_back_button_triggers():
    """
    Wire up on_root_back_button_click for every menu, once all widgets exist.
    Actions like `lvgl.widget.hide` on the menu itself need the widget map fully
    populated, so this must run as a deferred pass (mirrors trigger.py's
    generate_triggers()) rather than inline in MenuType.to_code -- doing it inline
    deadlocks the code-generation coroutine scheduler.
    """
    for w in get_widget_map().values():
        if not isinstance(w.type, MenuType):
            continue
        for conf in w.config.get(CONF_ON_ROOT_BACK_BUTTON_CLICK, ()):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await automation.build_automation(trigger, [], conf)
            async with LambdaContext(EVENT_ARG, where=conf[CONF_TRIGGER_ID]) as context:
                with LvConditional(
                    cg.RawExpression(
                        f"lvgl::lv_menu_back_button_is_root_click({w.obj}, event)"
                    )
                ):
                    lv_add(trigger.trigger())
            lv_obj.add_event_cb(
                w.obj, await context.get_lambda(), LV_EVENT.CLICKED, cg.nullptr
            )


def _record_set_page_action(config: dict) -> dict:
    """Remember the action so validate_menu_pages() can check the page's owner once
    the whole config is known."""
    get_menu_set_page_actions().append(config)
    return config


@automation.register_action(
    "lvgl.menu.set_page",
    ObjUpdateAction,
    cv.All(
        cv.Schema(
            {
                cv.Required(CONF_ID): cv.use_id(lv_menu_t),
                cv.Required(CONF_PAGE): cv.use_id(lv_menu_page_t),
                cv.Optional(CONF_SIDEBAR, default=False): cv.boolean,
            }
        ),
        _record_set_page_action,
    ),
    synchronous=True,
)
async def menu_set_page(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    page = await get_widget_(config[CONF_PAGE])
    sidebar = config[CONF_SIDEBAR]

    async def do_set_page(w: Widget):
        if sidebar:
            lv.menu_set_sidebar_page(w.obj, page.obj)
        else:
            lv.menu_set_page(w.obj, page.obj)

    return await action_to_code(widget, do_set_page, action_id, template_arg, args)
