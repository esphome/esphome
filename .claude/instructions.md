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
title: Component Name
description: Brief description of the component
---
```

**Important**:
- The `title` field becomes the H1 heading automatically
- **Do NOT** repeat the title as a `# Heading` in the markdown content
- The description should be concise (1-2 sentences)

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

## Markdown Syntax Standards

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
- See [WiFi](/components/wifi) for WiFi configuration
- Configure [I²C](/components/i2c) first
- Check out the [Deep Sleep](/components/deep_sleep) component
```

#### Custom Anchors
Use Hugo shortcodes for custom anchor points:
```markdown
{{< anchor "custom-section-name" >}}
```

Reference them with:
```markdown
See [this section](#custom-section-name) for details.
```

### Visual Elements

#### Images

**Optimization Requirements:**
- **ALWAYS optimize images** before adding (use TinyPNG/TinyJPG)
- **Maximum size**: ~1000x800 pixels
- All images must be as small as possible to minimize page load times

**Directory Structure:**
- **Component images**: Store in section-specific directories:
  - General components: `content/components/images/`
  - Sensor components: `content/components/sensor/images/`
  - Display components: `content/components/display/images/`
  - Binary sensors: `content/components/binary_sensor/images/`
  - Guides: `content/guides/images/`
  - Cookbook: `content/cookbook/images/`
  - And so on for each major section
- **Global/shared images and thumbnails**: Store in `static/images/`

**Referencing Images:**

**Default**: Use standard markdown syntax:
```markdown
![Alt text](filename.png)
```

The filename (no path) references the image in the section's images directory.

**When shortcode features are needed**: Use Hugo `img` shortcode for captions, width control, or alignment:
```markdown
{{< img src="filename.png" alt="Image description" caption="Optional caption text" width="75%" class="align-center" >}}
```

Shortcode parameters:
- `src`: Just the filename (no path) - looks in the section's images directory
- `alt`: Alternative text for accessibility
- `caption`: Descriptive caption (only available in shortcode)
- `width`: Percentage control like "75%", "50%" (only available in shortcode)
- `class`: Usually "align-center" for centering (only available in shortcode)

Examples:
```markdown
![Debug output](debug.png)

{{< img src="debug.png" alt="Debug component output" caption="Example debug component output." class="align-center" >}}
```

#### Component Thumbnails
- **Aspect ratio**: 8:10 or 10:8 (portrait or landscape orientation)
- **Format**: SVG (heavily compressed) or JPG (max 300x300px)
- **Location**: `static/images/` directory
- **Reference**: Use `/images/filename` path in frontmatter or links
- Used in component listings and cards

### Alerts and Callouts

Use GitHub-style alert syntax:

```markdown
> [!NOTE]
> This is an informational note for users.

> [!WARNING]
> This warns users about potential issues or breaking changes.

> [!TIP]
> This provides helpful tips and best practices.
```

**When to use**:
- `NOTE`: General information, clarifications, important context
- `WARNING`: Potential issues, breaking changes, compatibility notes
- `TIP`: Optimization suggestions, best practices, pro tips

## Git Workflow

### Branch Strategy
- **Bug fixes and documentation corrections**: Target the `current` branch
- **New features and new component docs**: Target the `next` branch
- **Create separate branches** for each pull request (one PR per feature/fix)

### Commit Messages
- Use clear, descriptive commit messages
- Format: `[component] Brief description of change`
- Examples:
  - `[dht] Fix temperature sensor example`
  - `[wifi] Add WPA3 configuration documentation`
  - `[docs] Update contributing guidelines`

### Pull Request Process
1. Ensure all changes are committed to the feature branch
2. Push to origin: `git push -u origin <branch-name>`
3. All automated tests must pass before review
4. Follow retry logic for network failures (exponential backoff: 2s, 4s, 8s, 16s)

## Testing and Preview

### Local Preview with Docker
To preview documentation locally:
```bash
docker run --rm -v "${PWD}/":/workspaces/esphome-docs -p 8000:8000 -it ghcr.io/esphome/esphome-docs
```

Then access the preview at `http://<HOST_IP>:8000`

### Before Submitting
- [ ] Check that all links work (internal and external)
- [ ] Verify code examples are syntactically correct
- [ ] Ensure images are optimized and properly sized
- [ ] Confirm line length is ≤120 characters
- [ ] Validate YAML frontmatter is present and correct
- [ ] Test that headings follow proper hierarchy (H2→H3→H4)
- [ ] Verify no H1 headings in content (title comes from frontmatter)

## Common Patterns

### Component Page Structure
A typical component page should follow this structure:

```markdown
---
title: Component Name
description: One-line description of what the component does
---

Brief introduction paragraph explaining what the component is and what it does.

## Configuration

Minimal configuration example here.

## Configuration Variables

List of all configuration variables with proper formatting.

## Examples

Additional examples showing common use cases.

## See Also

- Related components and guides
```

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

1. **Never duplicate the title** - it's automatically generated from frontmatter
2. **Start with H2**, not H1
3. **Wrap at 120 characters** maximum
4. **Use Title Case** for section headings
5. **Optimize images** before adding them
6. **Keep examples minimal** - only essential configuration
7. **Use literal `GPIOXX`** - the XX is literal, don't replace with numbers (unless fixed hardware)
8. **Link to dependencies** instead of including their config
9. **Target correct branch** (current vs next)
10. **GitHub CLI not available** - ask user for GitHub information if needed
