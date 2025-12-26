---
description: "Installing ESPHome Manually"
title: "Installing ESPHome Manually"
---

> [!WARNING]
> **Python 3.14 is not yet supported.** Please use Python 3.11, 3.12, or 3.13.
> Python 3.14 introduced breaking changes that ESPHome's dependencies have not yet adapted to.

## Windows

Download Python from [the official site](https://www.python.org/downloads/). Use Python 3.11, 3.12, or 3.13.

{{< img src="python-win-installer.png"
  alt="Python installer window with arrows pointing to \"Add Python to PATH\" and \"Install Now\""
  width="75.0%" class="align-center" >}}

Make sure you check "Add Python to PATH", and go all the way through the
installer.

Log out and back in, or restart your computer. Whichever is easiest.

Open the start menu and type `cmd`. Press the enter key.

In the terminal that comes up, check that Python is installed:

```shell
python --version
```

It should show something like:

```shell
Python 3.11.13
```

Looks good? You can go ahead and install ESPHome:

```shell
pip3 install wheel
pip3 install esphome
```

And you should be good to go! You can test that things are properly installed
with:

```shell
esphome version
```

It should show something like:

```shell
Version: 2025.8.0
```

> [!NOTE]
> You may additionally need to install git for the external components feature.
> Download git from [the official link](https://git-scm.com/downloads)

## Mac

ESPHome supports macOS. There are several ways to install ESPHome on macOS:

- Homebrew
- pip
- Cloning the repository

### Homebrew

An easy way for installation is via [Homebrew](https://brew.sh/):

```shell
brew install esphome
```

Verify the installation:

```shell
esphome version
```

It should show something like:

```shell
Version: 2025.8.0
```

> [!NOTE]
>
> - If you encounter any issues with Homebrew installation, please check the
>   [ESPHome Homebrew Formula](https://formulae.brew.sh/formula/esphome) page
>   for additional information.
>
> - Homebrew may not always provide the latest version immediately. Updating Homebrew will
>   automatically update ESPHome. If this is ok for you, Homebrew is the easiest way to
>   install ESPHome.

### pip

For the latest version, use the pip installation. This may be more difficult to set up
and may need additional dependencies and path settings. Setting up a virtual environment is
highly recommended. If you are not familiar with Python virtual environments, Homebrew
may be easier.

You will require Python 3.11 or newer. While your Mac may have a version of Python installed it may not be up-to-date.
Python can be installed from the [official site](https://www.python.org/downloads)
or with Homebrew. Once Python is installed, create and activate a virtual environment and install ESPHome with pip:

```shell
$ python3 -m venv venv      # The last argument is the folder in which to install the virtual environment
$ source venv/bin/activate  # For bash or compatible shells. If using a different shell, use activate.csh or activate.fish
(venv) $ pip install esphome       # Installs ESPHome in the virtual environment
(venv) $ esphome version
```

Any time you want to use ESPHome, you will need to have activated the virtual environment as shown above.
When activated you will see `(venv)` at the beginning of your prompt.

### Cloning the repository

For development purposes, we recommend cloning the repository. See our
[developer site](https://developers.esphome.io) for more information on setting up a development environment.

## Linux

Your distribution probably already has Python installed. Confirm that it is at
least version 3.11:

```shell
python3 --version
```

It should show something like:

```shell
Python 3.11.13
```

Looks good? Now create a virtual environment to contain ESPHome and it's dependencies.

```shell
python3 -m venv venv
source venv/bin/activate
```

You may or may not see `(venv)` at the beginning of your prompt depending on your shell configuration.
This indicates that you are in the virtual environment.

You can go ahead and install ESPHome:

```shell
pip3 install esphome
```

> [!CAUTION]
> Don't use `sudo` with pip. If you do, you'll run into trouble updating
> your Distro down the road.
>
> For details, see [DontBreakDebian](https://wiki.debian.org/DontBreakDebian#A.27make_install.27_can_conflict_with_packages).
> `pip install` is equivalent to `make install` in this context. The
> advice in the article applies to all Linux distributions, not just Debian.
>
> Some people install ESPHome without the virtual environment, which can lead to issues with PATHs etc.
> Installations without `venv` are considered not "supported" as people end up having to know your exact system setup.

At this point, you should be able to confirm that ESPHome has been successfully installed:

```shell
esphome version
```

It should show something like:

```shell
Version: 2025.8.0
```

If you get an error like "Command not found", you need to add the binary to
your `PATH` using `export PATH=$PATH:$HOME/.local/bin`.

To set this permanently, you can run ``echo 'export
PATH=$PATH:$HOME/.local/bin' >> $HOME/.bashrc``, then log out and back in.

## See Also

- {{< docref "/index" "ESPHome index" >}}
- {{< docref "getting_started_command_line/" >}}
- [Developer site](https://developers.esphome.io)
