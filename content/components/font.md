---
description: "Instructions for setting up fonts in ESPHome."
title: "Font Renderer Component"
params:
  seo:
    description: Instructions for setting up fonts in ESPHome.
    image: format-font.svg
---

{{< anchor "display-fonts" >}}

ESPHome's graphical rendering engine also has a powerful font drawer which integrates seamlessly into the system. You have the option to use **any** OpenType/TrueType (`.ttf`, `.otf`, `.woff`  ) font file at **any** size, as well as fixed-size [PCF](https://en.wikipedia.org/wiki/Portable_Compiled_Format) and [BDF](https://en.wikipedia.org/wiki/Glyph_Bitmap_Distribution_Format) bitmap fonts.

These fonts can be used in ESPHome's [own rendering engine](/components/display#display-engine) or in the {{< docref "/components/lvgl/index" "LVGL Graphics" >}} component.

To use fonts you can either

- Just grab a `.ttf`, `.otf`, `.woff`, `.pcf`, or `.bdf` file from somewhere on the internet and place it, for example, inside a `fonts` folder next to your configuration file.
- Use the `gfonts://` short form to use Google Fonts directly.
- Load a font from a URL directly on build.

Next, create a `font:` section in your configuration:

```yaml
# Various ways to configure fonts
font:
  - file: "fonts/Comic Sans MS.ttf"
    id: my_font
    size: 20
    bpp: 2

  - file: "fonts/tom-thumb.bdf"
    id: tomthumb

    # gfonts://family[@weight]
  - file: "gfonts://Roboto"
    id: roboto_20
    size: 20

  - file:
      type: gfonts
      family: Roboto
      weight: 900
    id: roboto_16
    size: 16

  - file: "gfonts://Material+Symbols+Outlined"
    id: icons_50
    size: 50
    glyphs: ["\U0000e425"] # mdi-timer

  - file: "fonts/RobotoCondensed-Regular.ttf"
    id: roboto_special_28
    size: 28
    bpp: 4
    glyphs: [
      0123456789aAáÁeEéÉ,
      (,),+,-,_,.,°,•,µ,
      "\u0020", # space
      "\u002C", # ,
      "\u0021", # !
      "\u0022", # "
      "\u0027", # '
      ]

  - file: "fonts/RobotoCondensed-Regular.ttf"
    id: my_font_with_icons
    size: 20
    bpp: 4
    extras:
      - file: "fonts/materialdesignicons-webfont.ttf"
        glyphs: [
          "\U000F02D1", # mdi-heart
          "\U000F05D4", # mdi-airplane-landing
          ]

  - file:
      type: gfonts
      family: Roboto
    id: roboto_european_core
    size: 16
    glyphsets:
      - GF_Latin_Core
      - GF_Greek_Core
      - GF_Cyrillic_Core

  - file: "https://github.com/IdreesInc/Monocraft/releases/download/v3.0/Monocraft.ttf"
    id: web_font
    size: 20
  - file:
      url: "https://github.com/IdreesInc/Monocraft/releases/download/v3.0/Monocraft.ttf"
      type: web
    id: web_font2
    size: 24

display:
  # ...
```

## Font metrics

The Component provides some useful font metrics. Those include:

- **ascender** (`get_ascender()`  ): The maximum height of the glyphs above the baseline (currently returns the same value as `get_baseline()`  ).

- **capheight** (`get_capheight()`  ): The height of the capital letters measured on the X glyph.

- **xheight** (`get_xheight()`  ): The height of the lowercase letters measured on the x glyph.

- **baseline** (`get_baseline()`  ): The imaginary line on which all characters sit.

- **descender** (`get_descender()`  ): The maximum height of the glyphs below the baseline.

- **linegap** (`get_linegap()`  ): The gap between two lines of text.

- **height** (`get_height()`  ): The lineheight of the font measured from baseline to baseline.

> [!NOTE]
> The `capheight` and `xheight` values are typically calculated using glyphs with flat tops.
> Rounded characters however might overshoot this value slightly to make them visually appear as the same size.
> For special fonts like the Material Design Icons font, which do not contain any letters, these two metrics will be set to 0.

The following code snipped produces the image below. Note that the lines in the code are ordered as they appear in the image from top to bottom.
For this font the `descender` and `height` are only one pixel apart.

```yaml
it.print(0, 0, id(my_font), "EspHome");

int ascender = id(my_font).get_baseline() - id(my_font).get_ascender();
int capheight = id(my_font).get_baseline() - id(my_font).get_capheight();
int xheight = id(my_font).get_baseline() - id(my_font).get_xheight();
int baseline = id(my_font).get_baseline();
int descender = id(my_font).get_baseline() + id(my_font).get_descender();
int height = id(my_font).get_height();

it.horizontal_line(0, ascender, it.get_width());
it.horizontal_line(0, capheight, it.get_width());
it.horizontal_line(0, xheight, it.get_width());
it.horizontal_line(0, baseline, it.get_width());
it.horizontal_line(0, descender, it.get_width());
it.horizontal_line(0, height, it.get_width());
```

{{< img src="fontmetrics.png" alt="Image" caption="The fontmetric values provided by ESPHome." width="60.0%" class="align-center" >}}

## Configuration variables

- **file** (**Required**, string): The path (relative to where the .yaml file is) of the font
  file. You can also use the `gfonts://` short form to use Google Fonts, or use the below structure:

  - **type** (**Required**, string): Can be `local`, `gfonts` or `web`.

  **Local Fonts**:

  - **path** (**Required**, string): The path (relative to where the .yaml file is) of the OpenType/TrueType or bitmap font file.

  **Google Fonts**:

    Each Google Font will be downloaded once and cached for future use. This can also be used to download Material
    Symbols or Icons as in the example above.

  - **family** (**Required**, string): The name of the Google Font family.
  - **italic** (*Optional*, boolean): Whether the font should be italic.
  - **weight** (*Optional*, enum): The weight of the font. Can be either the text name or the integer value:
    - **thin**: 100
    - **extra-light**: 200
    - **light**: 300
    - **regular**: 400 (**default**)
    - **medium**: 500
    - **semi-bold**: 600
    - **bold**: 700
    - **extra-bold**: 800
    - **black**: 900

  **Web Fonts**:

  - **url** (**Required**, string): The URL of the TrueType or bitmap font file.

- **id** (**Required**, [ID](/guides/configuration-types#id)): The ID with which you will be able to reference the font later
  in your display code.

- **size** (*Optional*, int): The desired size of the font. This will be the size (height) of the font in pixels
  when rendered. If you want to use the same font in different sizes, create two font objects.
  Defaults to `20` for scalable fonts and the first available size for bitmap fonts. Requesting a size that is not available in a bitmap-only font will result in an error.

- **bpp** (*Optional*, int): The bit depth of the rendered font from OpenType/TrueType, for anti-aliasing. Can be `1`, `2`, `4`, `8`. Defaults to `1`.
  If set to 1 and the font has a bitmap version available at the requested size, that will be used. Otherwise the font will be rendered from the vector representation.

- **glyphsets** (*Optional*, list): A list of glyphsets you plan to use. Defaults to
  `GF_Latin_Kernel`, which contains the basic characters and necessary punctuation and symbols for
  the English language. `GF_Latin_Core` is a more extended set that includes glyphs for the
  languages of Europe and the Americas with over 5 million speakers. Other options include
  `GF_Arabic_Core`, `GF_Cyrillic_Core`, `GF_Greek_Core`, their `Plus` variants, as well as
  `GF_Latin_African`, `GF_Latin_PriAfrican` and `GF_Latin_Vietnamese`.
  See the [Google Fonts Glyphset documentation](https://github.com/googlefonts/glyphsets/blob/main/GLYPHSETS.md)
  for an extensive list of all possible sets, along with their names and the languages supported by
  each of those sets. Note that `GF_Latin_Kernel` may need to be included for glyphs for basic
  characters such as numbers (0-9) and whitespace to be present.

- **glyphs** (*Optional*, list): A list of characters you plan to use, in addition to the characters
  defined by the **glyphsets** option above. Adjust this list if you need some special characters or
  want to reduce the size of the binary if you don't plan to use certain glyphs. Both single
  characters (`a, b, c`  ) or strings of characters (`abc, def`  ) are acceptable options. You can
  also specify glyphs by their codepoint (see below).

- **ignore_missing_glyphs** (*Optional*, boolean): By default, warnings are generated for any glyph
  that is included in the defined **glyphsets** but not present in the configured font. Use this
  setting to suppress those warnings. Please note that the absence of any manually defined glyphs
  (specified via the **glyphs** option) will always be treated as an error and will not be influenced
  by this setting.

- **extras** (*Optional*, enum): A list of font glyph configurations you'd like to include within this font, from other OpenType/TrueType files (eg. icons from other font, but at the same size as the main font):

  - **file** (**Required**, string): The path of the font file with the extra glyphs.
  - **glyphs** (**Required**, list): A list of glyphs you want to include. Can't repeat the same glyph codepoint if it was declared in the level above.

> [!NOTE]
> OpenType/TrueType font files offer icons at codepoints far from what's reachable on a standard keyboard, for these it's needed
> to specify the unicode codepoint of the glyph as a hex address escaped with `\u` or `\U`.
>
> - Code points up to `0xFFFF` are encoded like `\uE6E8`. Lowercase `\u` and exactly 4 hexadecimal digits.
> - Code points above `0xFFFF` are encoded like `\U0001F5E9`. Capital `\U` and exactly 8 hexadecimal digits.
>
> The `extras` section only supports OpenType/TrueType files, `size` and `bpp` will be the same as the above level. This will allow printing icons alongside the characters in the same string, like `I \uF004 You \uF001`.
>
> Many font sizes with multiple glyphs at high bit depths will increase the binary size considerably. Make your choices carefully.

> [!NOTE]
> To use fonts you will need to have the python `pillow` package installed, as ESPHome uses that package
> to translate the OpenType/TrueType and bitmap font files into an internal format. If you're running this as a Home Assistant add-on or with the official ESPHome docker image, it should already be installed. Otherwise you need
> to install it using `pip install "pillow==10.4.0"`.

## See Also

- {{< apiref "display/display_buffer.h" "display/display_buffer.h" >}}
- [Display Rendering Engine](/components/display#display-engine)
- {{< docref "/components/lvgl" >}}
- [MDI cheatsheet](https://pictogrammers.com/library/mdi/)
- [MDI font repository](https://github.com/Pictogrammers/pictogrammers.github.io/tree/main/%40mdi/font/)
- [Google Fonts Glyphsets](https://github.com/googlefonts/glyphsets/blob/main/GLYPHSETS.md)
