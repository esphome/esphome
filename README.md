# ESPHome-Docs

[![Netlify Status][netlify-badge]][netlify-link]
[![Discord Chat][discord-badge]][discord-link]
[![GitHub release][github-badge]][github-link]

[netlify-badge]: https://api.netlify.com/api/v1/badges/97a2e9ce-cee7-4cc8-8dc7-537c92a23fa7/deploy-status
[netlify-link]: https://app.netlify.com/sites/esphome/deploys
[discord-badge]: https://img.shields.io/discord/429907082951524364.svg
[discord-link]: https://discord.gg/KhAMKrd
[github-badge]: https://img.shields.io/github/release/esphome/esphome.svg
[github-link]: https://github.com/esphome/esphome/releases

<a href="https://esphome.io/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://esphome.io/images/logo-docs-on-dark.svg">
    <img src="https://esphome.io/images/logo-docs.svg" alt="ESPHome Logo">
  </picture>
</a>

This repository contains source for the documentation site for ESPHome.

## Project Structure

The project follows a standard directory structure:

``` text
esphome-docs/
├── archetypes/        # Content templates
├── assets/            # Source files for CSS, JS, etc.
├── content/           # Markdown content files
├── data/              # Data files for templates
├── layouts/           # HTML templates
├── static/            # Static files
│   └── images/        # Image files
├── themes/            # Custom theme for ESPHome
│   └── esphome-theme/ # The ESPHome custom theme
├── hugo.yaml          # Hugo configuration
└── README.md          # This file
```

## Image File Resolution

Images in the Hugo site referred to in `img` shortcodes are handled using a specific search strategy:

### Relative paths

- When using relative paths in the `img` shortcode (e.g., `{{< img src="dht22.jpg" >}}`),
  Hugo will first look in a local `images/` subdirectory.
  For example, an image referenced in `content/components/sensor/dht.md` will first be
  searched for in `content/components/sensor/images/`.

- If the image is not found in the local directory, Hugo will then look in the global `/static/images/` directory.

### Absolute paths

When using absolute paths (starting with `/`), Hugo will look directly in the specified location
relative to the `/static/` directory.

This strategy allows component documentation to have its own images while also supporting shared images across the site.

## Custom Theme

The site uses a custom theme called `esphome-theme` which is designed to match the look and feel of the original
ESPHome documentation. The theme includes:

- Responsive design for mobile and desktop
- Dark mode support
- Custom shortcodes for documentation features
- Navigation sidebar
- Search functionality

## Markdown

Hugo uses Markdown files as input. The Markdown processor in use is Goldmark.

## Hugo Template System

Hugo uses a templating system to generate HTML from Markdown content. Understanding the following concepts is helpful
when working with or modifying the theme:

### Templates

Templates are HTML files with Go templating syntax that define the structure and layout of pages. Hugo uses different
types of templates:

- **Base Templates**: Define the overall structure of the site (found in `layouts/_default/baseof.html`)
- **List Templates**: Used for section pages that list multiple content items
- **Single Templates**: Used for individual content pages
- **Home Template**: Specifically for the homepage

Templates use blocks (like `{{ block "main" . }}{{ end }}`) that can be overridden by other templates.

### Partials

Partials are reusable template components that can be included in other templates. They help maintain DRY (Don't Repeat
Yourself) code by extracting common elements:

``` text
{{ partial "header.html" . }}
```

The dot (`.`) passes the current context to the partial. Partials are stored in the `layouts/partials/` directory.

### Shortcodes

Shortcodes are special tags you can use within Markdown content to insert complex elements or custom HTML.
They bridge the gap between the simplicity of Markdown and the need for more complex formatting.

``` text
{{< shortcode-name param1="value" param2="value" >}}
```

Shortcodes can be self-closing or can wrap content:

``` text
{{< shortcode-name >}}
  Content to be processed
{{< /shortcode-name >}}
```

Shortcode templates are stored in the `layouts/shortcodes/` directory.

## Shortcodes

Hugo has a number of [built-in shortcodes](https://gohugo.io/content-management/shortcodes/) and the ESPHome theme
also defines several custom shortcodes:

### `anchor`

Creates an HTML anchor point that can be linked to with fragment identifiers.

``` text
{{< anchor "my-anchor-id" >}}
```

NOTE: Headings automatically create anchors, so it is not necessary to insert `anchor` shortcodes for them.

### `collapse`

Creates a collapsible section with a title that can be clicked to show/hide content.

``` text
{{< collapse true >}}
This content will be hidden by default and can be expanded by clicking the header.
You can include any Markdown content here, including lists, code blocks, etc.
the parameter, if present and set to true, will have the content initially opened. Note that False is a truthy string, not a boolean
{{< /collapse >}}
```

### `docref`

Creates a link to another page in the documentation with proper handling of anchors.

``` text
{{< docref "/components/sensor/dht" >}}                     <!-- Uses the target page title as link text -->
{{< docref "/components/sensor/dht" "DHT Sensor Guide" >}}  <!-- Uses custom text for the link -->
{{< docref "/components/sensor/dht#configuration" >}}       <!-- Links to a specific anchor on the page -->
```

### `img`

Displays an image with optional caption, width, height, and CSS class.

``` text
{{< img src="example.jpg" alt="Example image" caption="This is an example" width="500" class="center" >}}
```

### `imgtable`

Creates a component card with an image, title, and optional description that links to another page.

``` text
  {{< imgtable >}}
  Title 1, path/to/page1, image1.png
  Title 1, path/to/page1, image1.png, "sub-caption"
  Title 2, path/to/page2, image2.png, dark-invert
  Title 2, path/to/page2, image2.png, "sub-caption", dark-invert
  {{< /imgtable >}}
  
```

### `seo`

Adds SEO metadata tags to the page for better search engine optimization and social media sharing.

``` text
{{< seo description="Detailed guide for setting up the DHT sensor with ESPHome" image="dht-sensor.jpg" >}}
```

### `apiref`

Creates a link to a C++ API header file.

``` text
{{< apiref "Component" "esphome/core/component.h" >}}
```

### `apiclass`

Creates a link specifically to a C++ class in the API documentation.

``` text
{{< apiclass "ClimateDevice" "esphome::climate::ClimateDevice" >}}
{{< apiclass "WiFiComponent" "esphome::wifi::WiFiComponent" >}}
```

### `apistruct`

Creates a link specifically to a C++ struct in the API documentation.

``` text
{{< apistruct "SensorStateClass" "esphome::sensor::SensorStateClass" >}}
{{< apistruct "GPIOOutputPin" "esphome::output::GPIOOutputPin" >}}
```

### `api-key-input`

Creates an input field with a randomly generated API key and a copy button.

``` text
{{< api-key-input >}}
```

### `ghuser`

Creates a link to a GitHub user profile.

``` text
{{< ghuser name="octocat" >}}                <!-- Links to @octocat -->
{{< ghuser name="octocat" text="GitHub" >}}  <!-- Links to @octocat but displays "GitHub" -->
```

### `html_file`

Reads a file from the static directory and inserts it as HTML.

``` text
{{< html_file file="example.html" class="example-class" >}}
```

### `option`

Creates an option block for documenting command-line options or configuration parameters.

``` text
{{< option "--help|-h" >}}
This is the help option.
{{< /option >}}
```

### `pr`

Creates a link to a GitHub pull request.

``` text
{{< pr number="123" >}}                <!-- Links to esphome/esphome#123 -->
{{< pr number="123" repo="esphome-docs" >}}    <!-- Links to esphome/esphome-docs#123 -->
```

### `redirect`

Creates a page that automatically redirects to another URL.

``` text
{{< redirect url="/some/path" >}}
```

## Markdown features

### `note`

Creates a note blockquote/alert box to highlight important information.

> [!NOTE]
> This is important information that the reader should pay attention to.
> You can include **Markdown** formatting within the block.

```markdown
> [!NOTE]
> This is important information that the reader should pay attention to.
> You can include **Markdown** formatting within the block.
```

### `important`

Creates an important blockquote/alert box to highlight helpful information.

> [!IMPORTANT]
> This is helpful information that the reader should be aware of.
> You can include **Markdown** formatting within the block.

```markdown
> [!IMPORTANT]
> This is helpful information that the reader should be aware of.
> You can include **Markdown** formatting within the block.
```

### `tip`

Creates a tip blockquote/alert box to highlight helpful advice or best practices.

> [!TIP]
> For best results, place the sensor away from heat sources.
> You can include **Markdown** formatting within the block.

```markdown
> [!TIP]
> For best results, place the sensor away from heat sources.
> You can include **Markdown** formatting within the block.
```

### `warning`

Creates a warning blockquote/alert box to highlight important warnings or potential issues.

> [!WARNING]
> Incorrect wiring may damage your device. Double-check connections before powering on.
> You can include **Markdown** formatting within the block.

```markdown
> [!WARNING]
> Incorrect wiring may damage your device. Double-check connections before powering on.
> You can include **Markdown** formatting within the block.
```

### `caution`

Creates a caution blockquote/alert box to highlight important cautions or potential issues.

> [!CAUTION]
> Incorrect wiring may damage your device. Double-check connections before powering on.
> You can include **Markdown** formatting within the block.

```markdown
> [!CAUTION]
> Incorrect wiring may damage your device. Double-check connections before powering on.
> You can include **Markdown** formatting within the block.
```

## Conversion Scripts

A Python script is included to help with the conversion process from RST:

`script/convert_rst_to_md.py` - Converts Sphinx RST files to Hugo Markdown format
Available options for convert_rst_to_md.py:

``` text
positional arguments:
  input_dir             Input directory containing RST files
  output_dir            Output directory for Markdown files

optional arguments:
  --single FILENAME     Process a single file (relative to input_dir)
  --no-images           Skip image processing
```

The script performs the following operations:

- Builds an anchor map to maintain internal links
- Converts RST formatting to Markdown
- Processes special directives like notes, warnings, and tips
- Converts RST tables to Markdown format
- Handles image references and copies images to appropriate locations
- Processes inline markup and references

### Converting a PR that was written with RST

See the `script/convert-pr.sh` script for converting a PR that was written with RST.

## Development

To run the site locally:

1. Install Hugo: <https://gohugo.io/installation/>
1. Install NodeJS (simplest way to run pagefind)
1. Clone this repository
1. Navigate to the repository directory
1. Run `make live-html`
1. Open your browser to <http://localhost:8000/>

## Building for Production

See the GitHub workflows in `.github/workflows`

The built site will be in the `public` directory.

## Contributing

Contributions to improve the documentation are welcome! Please follow these steps:

1. Fork the repository
1. Create a new branch for your changes
1. Make your changes
1. Submit a pull request

## License

The ESPHome documentation is licensed under the
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][by-ns-sa].

**Documentation:** <https://esphome.io/>

For issues, please go to [the issue tracker](https://github.com/esphome/esphome/issues).

For feature requests, please see [feature requests](https://github.com/orgs/esphome/discussions).

[by-ns-sa]: https://creativecommons.org/licenses/by-nc-sa/4.0/
