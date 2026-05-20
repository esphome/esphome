# ESPHome Documentation Instructions

## Documentation Style & Formatting

### Language and Text Formatting

- **Always use English** for all documentation
- **Section titles**: Use Title Case formatting (e.g., "Configuration Variables", "Getting Started")
- **Line length**: Wrap lines at **maximum 120 characters** for readability
- **Tone**: Be clear, concise, and technical. Use present tense and active voice where possible

### Content Guidelines

#### Component Documentation

- Provide **minimal examples** that exclude optional configuration variables
- Examples should focus on the essential configuration only
- When components have dependencies:
  - Include a sentence explaining the dependency
  - Provide a link to the dependency's documentation
  - Do NOT include the dependent component's configuration in the example

#### Pin References

- **ALWAYS use the literal string `GPIOXX`** in documentation examples
- The `XX` is literal - do NOT replace with actual numbers like `GPIO16`
- **Exception**: Only use specific pin numbers when documenting hardware with fixed pins
- This ensures examples work across different boards and users replace with their actual pins

## File Structure & Format

### Frontmatter Requirements

Every documentation page **must** start with YAML frontmatter:

```yaml
---
title: "Component Name"
description: "Brief description of the component"
---
```

**Important**:

- The `title` field becomes the H1 heading automatically
- **Do NOT** repeat the title as a `# Heading` in the markdown content
- The description should be concise (1-2 sentences)
- Both title and description should be quoted strings

### Heading Hierarchy

- **Start with H2 (`##`)** for main sections
- Never use H1 (`#`) in the content (it's generated from frontmatter)
- Use proper nesting: H2 → H3 → H4
- Examples:

  ```markdown
  ## Configuration Variables

  ### Required Options

  #### Advanced Settings
  ```

## MDX Syntax Standards

### File Format

- Files use **MDX format** (`.mdx` extension) which allows JSX components in Markdown
- Import required components at the top of the file after frontmatter

### Code Formatting

#### Inline Code

- Use single backticks for inline code: `variable_name`, `sensor`, `GPIOXX`
- Use for: component names, variable names, short code snippets, pin numbers

#### Code Blocks

- Use triple backticks with language identifier:

```yaml
sensor:
  - platform: dht
    pin: GPIOXX
    temperature:
      name: "Living Room Temperature"
```

- Common language identifiers: `yaml`, `cpp`, `python`, `bash`

### Configuration Variables

Use special formatting to indicate parameter requirements:

- **Config key**: Always bold
- **Required** label: Bold
- *Optional* label: Italics

Example:

```markdown
- **pin** (**Required**, Pin): The pin where the sensor is connected.
- **update_interval** (*Optional*, Time): The interval to check the sensor. Defaults to `60s`.
```

### Links

#### External Links

Standard markdown syntax:

```markdown
[ESP32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
```

#### Internal Documentation Links

Use **relative paths** starting with `/`:

```markdown
- See [WiFi](/components/wifi/) for WiFi configuration
- Configure [I²C](/components/i2c/) first
- Check out the [Deep Sleep](/components/deep_sleep/) component
```

**Important**: Always include trailing slash `/` for internal links.

#### Anchors

Headings automatically create anchors using their slugified form. To link to a specific section:

```markdown
See [Configuration Variables](#configuration-variables) for details.
```

Wherever possible, use automatic anchors generated from headings. Only when it is not feasible to use automatic anchors should you use HTML:

```markdown
<span id="custom-section-name"></span>
```

### Visual Elements

#### Images

**Directory Structure and Import Strategy:**

1. **Single-use images** (used in only one file):

   - Store in a local `images/` directory next to the MDX file
   - Import at the top of the file
   - Use imported image reference

   ```mdx
   import { Image } from 'astro:assets';
   import myImageImg from './images/my-image.jpg';

   <Image src={myImageImg} alt="Description" layout="constrained" />
   ```

1. **Multi-use images** (used in multiple files):

   - Store in `/public/images/`
   - Reference using absolute path

   ```mdx
   <Image src="/images/shared-image.jpg" alt="Description" layout="constrained" />
   ```

1. **ImgTable images** (component index pages):

   - **MUST** be in `/public/images/`
   - ImgTable component resolves to `/images/filename.png`

**Image Optimization:**

- **ALWAYS optimize images** before adding (use TinyPNG/TinyJPG)
- **Maximum size**: ~1000x800 pixels
- All images must be as small as possible to minimize page load times
- Variable names follow camelCase with `Img` suffix (e.g., `myImageImg`)

**Image Attributes:**

- `src`: Either imported variable or absolute path
- `alt`: Alternative text for accessibility (required)
- `layout`: Use `"constrained"` for responsive images
- `width` and `height`: Optional pixel values (not percentages)

**Styling Images:**

Use inline styles for sizing:

```mdx
<Image
  src={myImageImg}
  alt="Description"
  layout="constrained"
  style="width: 50%; height: auto; margin: 0 auto; display: block;"
/>
```

#### Component Thumbnails

- **Aspect ratio**: 8:10 or 10:8 (portrait or landscape orientation)
- **Format**: SVG (heavily compressed) or JPG (max 300x300px)
- **Location**: `/public/images/` directory
- **Reference**: Use `/images/filename` path in ImgTable
- Used in component listings and cards

### Alerts and Callouts

Use GitHub-style alert syntax:

```markdown
> [!NOTE]
> This is an informational note for users.

> [!IMPORTANT]
> This provides important information that should be emphasized.

> [!TIP]
> This provides helpful tips and best practices.

> [!WARNING]
> This warns users about potential issues or breaking changes.

> [!CAUTION]
> This warns about critical issues that could cause damage.
```

**When to use**:

- `NOTE`: General information, clarifications, important context
- `IMPORTANT`: Important information that needs emphasis
- `TIP`: Optimization suggestions, best practices, pro tips
- `WARNING`: Potential issues, breaking changes, compatibility notes
- `CAUTION`: Critical warnings that could cause damage or data loss

### Mathematical Expressions

Use LaTeX syntax with KaTeX for equations:

**Inline math:**

```markdown
The formula is $E = mc^2$ where...
```

**Block equations:**

```markdown
$$
\text{formula} = \frac{a}{b}
$$
```

### Custom Components

#### APIRef Component

Link to C++ API documentation:

```mdx
import APIRef from '@components/APIRef.astro';

<APIRef text="component.h" path="component/component.h" />
```

#### ImgTable Component

Create grids of component cards:

```mdx
<ImgTable items={[
  ["Title", "/path/to/page/", "image.png"],
  ["Title 2", "/path/to/page2/", "image2.png", "caption"],
  ["Title 3", "/path/to/page3/", "image3.png", "caption", "dark-invert"],
]} />
```

**Important**: All ImgTable images must be in `/public/images/`.

#### Figure Component

Add captions to images using the Figure component:

**Simple text caption (using caption prop):**

```mdx
import Figure from '@components/Figure.astro';
import myImageImg from './images/my-image.jpg';

<Figure
  src={myImageImg}
  alt="Description"
  caption="This caption appears below the image"
/>
```

**Caption with markdown links (using caption slot):**

```mdx
import Figure from '@components/Figure.astro';

<Figure
  src="/images/badge.svg"
  alt="Badge"
>
  <p slot="caption">
    Download: [svg](/images/badge.svg), [png](/images/badge.png)
  </p>
</Figure>
```

**With absolute path for multi-use images:**

```mdx
<Figure
  src="/images/shared-image.jpg"
  alt="Description"
  caption="Optional caption text"
  style="width: 50%"
/>
```

**Props:**

- `src`: Either imported variable or absolute path (required)
- `alt`: Alternative text for accessibility (required)
- `caption`: Optional caption text for simple captions (string)
- `layout`: Use `"constrained"` for responsive images (default)
- `style`: Optional inline CSS styles
- `width`, `height`: Optional pixel values

**Slots:**

- `caption`: For captions with markdown/links (use `<Fragment slot="caption">` with markdown content)

## Git Workflow

### Branch Strategy

- **Bug fixes and documentation corrections**: Target the `current` branch
- **New features and new component docs**: Target the `next` branch
- **Create separate branches** for each pull request (one PR per feature/fix)
- **Always branch from the target branch**: Use `git checkout -b <branch-name> current` or `git checkout -b <branch-name> next` to ensure you're branching from the correct base

### Commit Messages

- Use clear, descriptive commit messages
- Format: `[component] Brief description of change`
- Examples:
  - `[dht] Fix temperature sensor example`
  - `[wifi] Add WPA3 configuration documentation`
  - `[docs] Update contributing guidelines`

### Pull Request Process

1. Ensure all changes are committed to the feature branch
1. Push to origin: `git push -u origin <branch-name>`
1. Create the PR using the `.github/PULL_REQUEST_TEMPLATE.md` template - fill out all sections completely
1. All automated tests must pass before review
1. For new components, update `/src/content/docs/components/index.mdx`

## Testing and Preview

### Local Preview

To preview documentation locally:

```bash
npm install
npm run dev
```

Then access the preview at `http://localhost:4321/`

### Building for Production

```bash
npm run build
```

### Before Submitting

- [ ] Check that all links work (internal and external with trailing slashes)
- [ ] Verify code examples are syntactically correct
- [ ] Ensure images are optimized and properly sized
- [ ] Confirm line length is ≤120 characters
- [ ] Validate YAML frontmatter is present and correct
- [ ] Test that headings follow proper hierarchy (H2→H3→H4)
- [ ] Verify no H1 headings in content (title comes from frontmatter)
- [ ] Verify images are imported correctly (single-use) or in /public/images/ (multi-use)
- [ ] Check that ImgTable images are in /public/images/

## Common Patterns

### Component Page Structure

A typical component page should follow this structure:

```mdx
---
title: "Component Name"
description: "One-line description of what the component does"
---

import { Image } from 'astro:assets';
import myImageImg from './images/my-image.jpg';
import APIRef from '@components/APIRef.astro';

Brief introduction paragraph explaining what the component is and what it does.

<Image src={myImageImg} alt="Component image" layout="constrained" />

## Configuration

```yaml
# Minimal configuration example here
```

## Configuration Variables

List of all configuration variables with proper formatting.

## Examples

Additional examples showing common use cases.

## See Also

- [Related Component](/components/related/)

Note: APIRef component would be used here but cannot be shown in markdown example.

### Writing Configuration Examples

**Good example** (minimal, focused):

```yaml
sensor:
  - platform: dht
    pin: GPIOXX
    temperature:
      name: "Temperature"
```

**Bad example** (includes unnecessary optional parameters):

```yaml
sensor:
  - platform: dht
    pin: GPIOXX
    model: AUTO_DETECT
    update_interval: 60s
    temperature:
      name: "Temperature"
      id: temp_sensor
      unit_of_measurement: "°C"
      accuracy_decimals: 1
```

## Key Reminders

- **Files use MDX format** (`.mdx` extension)
- **Import components** at the top after frontmatter
- **Never duplicate the title** - it's automatically generated from frontmatter
- **Start with H2**, not H1
- **Wrap at 120 characters** maximum
- **Use Title Case** for section headings
- **Optimize images** before adding them
- **Single-use images**: Import from `./images/`
- **Multi-use images**: Absolute path from `/public/images/`
- **ImgTable images**: Must be in `/public/images/`
- **Keep examples minimal** - only essential configuration
- **Use literal `GPIOXX`** - the XX is literal, don't replace with numbers (unless fixed hardware)
- **Link to dependencies** instead of including their config
- **Target correct branch** (current vs next)
- **Internal links**: Include trailing slash (e.g., `/components/wifi/`)
- **GitHub CLI not available** - ask user for GitHub information if needed
