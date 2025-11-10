---
description: "Instructions for setting up ESPHome External Components."
title: "External Components"
params:
  seo:
    description: Instructions for setting up ESPHome External Components.
---

You can easily import community or personal components using the external components feature.
Bundled components can be overridden using this feature.

If you want to create your own components/platforms for ESPHome, please see our
[developer site](https://developers.esphome.io).

```yaml
external_components:
  # use rtttl and dfplayer from ESPHome's dev branch in GitHub
  - source:
      type: git
      url: https://github.com/esphome/esphome
      ref: dev
    components: [ rtttl, dfplayer ]

  # equivalent shorthand for GitHub
  - source: github://esphome/esphome@dev
    components: [ rtttl ]

  # equivalent shorthand for GitHub pull request
  - source: github://pr#2639
    components: [ rtttl ]

  # use all components from a local folder
  - source:
      type: local
      path: my_components

  # use a component from a local git repository
  - source:
      type: git
      url: file:///Users/user/path_to_repo
      ref: my_awesome_branch
    components: [my_awesome_component]
```

## Configuration variables

- **source**: The location of the components you want to retrieve. See [Local](#external-components_local)
  and [external-components_git](#external-components_git).

  - **type** (**Required**): Repository type. One of `local`, `git`.

  git options:

  - **url** (**Required**, url): Git repository url. See [external-components_git](#external-components_git).
  - **ref** (*Optional*, string): Git ref (branch or tag). If not specified the default branch is used.
  - **username** (*Optional*, string): Username for the Git server, if one is required
  - **password** (*Optional*, string): Password for the Git server, if one is required
  - **path** (*Optional*, string): Path inside the repo, if different from `components` or `esphome/components`

  local options:

  - **path** (**Required**): Path to use when using local components. See [Local](#external-components_local).

- **components** (*Optional*, list): The list of components to use from the external source.
  By default, all available components are used.

- **refresh** (*Optional*, [Time](/guides/configuration-types#time)): The interval the source will be checked. Has no
  effect on `local`. See [Refresh](#external-components_refresh). for more info. Defaults to `1day`.

{{< anchor "external-components_local" >}}

## Local

You can specify a local path containing external components. This is most useful when developing a
component or if you want to manually control the origin of the files.

```yaml
external_components:
  - source:
      path: /copied_components

# shorthand
external_components:
  - source: my_components
```

Notice that relative paths are supported, so you can enter `my_components` as the source path and then
ESPHome will load components from a `my_components` folder in the same folder where your YAML configuration
is.

### Example of local components

Given the above example of `my_components`, the folder structure must look like:

```text
<CONFIG_DIR>
├── node1.yaml
├── node2.yaml
└── my_components
    ├── my_component1
    │   ├── __init__.py
    │   ├── component1.cpp
    │   ├── component1.h
    │   └── sensor.py
    └── my_component2
        ├── __init__.py
        ├── component2.cpp
        ├── component2.h
        └── switch.py
```

{{< anchor "external-components_git" >}}

## Git

Retrieving components from git is the easiest way to use components not included in ESPHome by default.
The source components should be inside a `components` folder or inside an `esphome/components`
folder. The latter makes sharing a component from a forked ESPHome repository easier.

The url to the repository may be remote (`http:` or `https:` scheme) or local (`file:` scheme with an absolute path.)

### Example of git repositories

For repositories where you share one or a few components:

```text
components
├── my_component1
│   ├── __init__.py
│   ├── component1.cpp
│   ├── component1.h
│   └── sensor.py
└── my_component2
    ├── __init__.py
    ├── component2.cpp
    ├── component2.h
    └── switch.py
example_component1.yaml        <- not required but recommended
README.md
```

or, this structure is also supported, which makes handy to share components from a **forked** ESPHome
repository:

```text
esphome
├── components
│   ├── my_component1
│   │   ├── __init__.py
│   │   ├── component1.cpp
│   │   ├── component1.h
│   │   └── sensor.py
│   ├── my_component2
│   │   ├── __init__.py
│   │   ├── component2.cpp
│   │   ├── component2.h
│   │   └── switch.py
│  ...
...
```

HTTP git repositories in general are supported with this configuration:

```yaml
external_components:
  source:
    type: git
    url: http://repository_url/
    ref: branch_or_tag
  source:
    type: git
    url: file:///Users/user/path_to_repo
    ref: branch_or_tag
```

The source field accepts a short hand **github://** resource:

```yaml
external_components:
  # shorthand
  source: github://<user or org>/<repository name>[@<branch or tag>]
```

The source field also accepts a short hand **github://** pull request from the ESPHome repository:

```yaml
external_components:
  # shorthand
  source: github://pr#<number>
```

Under the hood, during validation, ESPHome will clone the git repository into the hidden `.esphome`
folder and components will then be loaded from this local copy. The local path of the cloned repository
varies per repository name and ref name, so repositories with different refs are considered different
repositories and updated independently.

If required, you can supply a username and password to use when authenticating with the remote git
server using the `username` and `password` fields. This is most useful when combined with the
`!secret`  feature, to load the values in from a `secrets.yaml` file. This is not a comprehensive
security measure; your username and password will necessarily be stored in clear text within the
`.esphome` directory.

{{< anchor "external-components_refresh" >}}

### Refresh

Components are initially cloned into a cache directory, then the repository is checked for updates
(via *git pull*) after the `refresh:` time passes since last check.

You can make ESPHome check the repository every time by setting this option to `0s`, however since
ESPHome is validating the configuration continuously while using the dashboard or the vscode extension,
it is not recommended to set this value to less than a few minutes to avoid validation slow down and
excessive repository checks.

Likewise, you can set this setting to `never` and ESPHome will never
**update** the repository, useful e.g. when `ref` points to a **tag**.

## See Also
