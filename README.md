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
- A C compiler, `pkg-config`, and the `rofi` development files during
  installation (the Arch Linux `rofi` package includes them)
- GNU `find`, `realpath`, `sort`, `mv`, and `sha256sum`
- `send2trash` or `trash` for safely removing confirmed duplicates
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
~/.local/share/fuzzy-mover/fuzzy-mover.so
```

The second path is rooted at `$XDG_DATA_HOME` when that variable is set.

It generates an absolute `Exec` path in the installed Service Menu, so Dolphin
does not need `~/.local/bin` in its `PATH`.

Restart Dolphin after installation. One way to do that is:

```bash
kquitapp6 dolphin
dolphin >/dev/null 2>&1 &
```

Running `./install.sh` again updates all installed components without changing
the configured Photos root. Run it again after a `rofi` upgrade as well, so the
picker plugin is rebuilt for the installed `rofi` version.

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

To hide destination directories from the picker, put one POSIX extended regular
expression per line in:

```text
~/.config/fuzzy-mover/exclude-regexes
```

Each expression is matched against individual directory names, not the entire
relative path. A matching directory and all of its descendants are excluded.
Empty lines and lines beginning with `#` are ignored. For example, this hides
directories whose names begin with an uppercase `S`, one or more digits, and a
space:

```text
^S[[:digit:]]+[[:space:]]
```

An invalid expression stops the operation with an error instead of silently
showing an incomplete destination list. `FUZZY_MOVER_EXCLUDE_FILE` can point to
an alternative file; point it to `/dev/null` to disable exclusions temporarily.

The picker uses rofi's `Arc-Dark` theme by default. To choose another installed
rofi theme, put its name in:

```text
~/.config/fuzzy-mover/rofi-theme
```

For example:

```bash
printf '%s\n' 'gruvbox-dark' \
    > "${XDG_CONFIG_HOME:-$HOME/.config}/fuzzy-mover/rofi-theme"
```

An absolute path to a custom `.rasi` file also works. Use an empty file to let
rofi use its global theme instead. `FUZZY_MOVER_ROFI_THEME` overrides this
setting.

## Usage

1. Select one or more images in Dolphin.
2. Right-click and choose **Move to Photos…**, or use its keyboard shortcut.
3. Type any fuzzy query, use the arrow keys to select a directory, and press
   Enter.

Results are ranked first by the longest contiguous part of the query found in
the path. Remaining ties are handled by `rofi`. For example, `__ungrouped`
ranks ahead of `unknown green` for the query `ungr`, while fully fuzzy queries
such as `conslov` continue to work.

On Dolphin 26.04 or newer, assign a shortcut under:

**Settings → Configure Keyboard Shortcuts… → Context Menu Actions → Move to Photos…**

`Alt+M` is a convenient choice if it is unused in your setup.

## Safety behavior

- Existing destination files are never intentionally overwritten. If a path
  with the same name already exists, both files are compared using SHA-256.
  An identical source is sent to the system Trash; a different source is moved
  under the first available numbered name, for example `image (1).png`, then
  `image (2).png`. Occupied numbered names are checked for duplicates too.
- A destination symlink leading outside the configured Photos tree is rejected.
- Moves use `mv` directly and duplicate removal prefers `send2trash`, falling
  back to `trash` when necessary, so the operation is not registered in
  Dolphin's Undo history. Trashed duplicates can be recovered from the system
  Trash until it is emptied. Keep a backup of irreplaceable recovered files.

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
