# fuzzy-mover

`fuzzy-mover` is a helper tool created for my own needs to quickly sort recovered photos that landed in a single folder
without original names and structure.
It adds a **Move to Photos…** action to Dolphin. Select one or more
images, invoke the action, fuzzy-search the directory tree under `Photos`, and
press Enter to move the files there.

Destination entries use paths relative to the Photos root, for example:

```text
Construction/Slovakia
Construction/Poland 2015
Holidays/Slovakia
```

This means repeated leaf-directory names remain unambiguous, while a query such
as `conslov` can still match `Construction/Slovakia`.

## Requirements

- Dolphin and KDE Frameworks/KIO
- Bash
- `rofi`
- GNU `find`, `realpath`, `sort`, and `mv`
- `notify-send` is optional but recommended for completion and error messages

Keyboard shortcuts for Service Menu actions require Dolphin 26.04 or newer.

## Installation

Install for the current user; no `sudo` is needed:

```bash
./install.sh
```

The installer writes:

```text
~/.local/bin/fuzzy-move
~/.local/share/kio/servicemenus/fuzzy-move.desktop
```

The second path is rooted at `$XDG_DATA_HOME` when that variable is set.

It generates an absolute `Exec` path in the installed Service Menu, so Dolphin
does not need `~/.local/bin` in its `PATH`.

Restart Dolphin after installation. One way to do that is:

```bash
kquitapp6 dolphin
dolphin >/dev/null 2>&1 &
```

## Configuration

The default Photos root is `~/Photos`. To use another directory, put its path
as the only line in:

```text
~/.config/fuzzy-mover/photos-root
```

The path is rooted at `$XDG_CONFIG_HOME` when that variable is set.

For example:

```bash
mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/fuzzy-mover"
printf '%s\n' '/mnt/archive/Photos' \
    > "${XDG_CONFIG_HOME:-$HOME/.config}/fuzzy-mover/photos-root"
```

A leading `~` is supported. The `FUZZY_MOVER_PHOTOS_ROOT` environment variable
overrides both the configuration file and the default.

## Usage

1. Select one or more images in Dolphin.
2. Right-click and choose **Move to Photos…**, or use its keyboard shortcut.
3. Type any fuzzy query, use the arrow keys to select a directory, and press
   Enter.

On Dolphin 26.04 or newer, assign a shortcut under:

**Settings → Configure Keyboard Shortcuts… → Context Menu Actions → Move to Photos…**

`Alt+M` is a convenient choice if it is unused in your setup.

## Safety behavior

- Existing destination files are never intentionally overwritten. A source
  file with a colliding name is left in place and counted as skipped.
- A destination symlink leading outside the configured Photos tree is rejected.
- The operation uses `mv` directly, so it is not registered in Dolphin's Undo
  history. Keep a backup of irreplaceable recovered files.

## Uninstallation

Remove the executable and Dolphin Service Menu while preserving configuration:

```bash
./uninstall.sh
```

Remove the configuration as well:

```bash
./uninstall.sh --purge-config
```

Restart Dolphin afterwards. The uninstaller only removes fuzzy-mover's known
files; it does not remove `Photos` or any image files.
