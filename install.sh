#!/usr/bin/env bash

set -eu

usage() {
    cat <<'EOF'
Usage: ./install.sh

Installs fuzzy-mover for the current user. No sudo access is required.

Optional environment variables:
  FUZZY_MOVER_BIN_DIR      Executable directory (default: ~/.local/bin)
  XDG_DATA_HOME            XDG data directory (default: ~/.local/share)
EOF
}

die() {
    printf 'fuzzy-mover: %s\n' "$1" >&2
    exit 1
}

case "${1:-}" in
    "") ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
bin_dir="${FUZZY_MOVER_BIN_DIR:-$HOME/.local/bin}"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
service_dir="$data_home/kio/servicemenus"
installed_executable="$bin_dir/fuzzy-move"
installed_service="$service_dir/fuzzy-move.desktop"
installed_root_service="$service_dir/fuzzy-mover-set-root.desktop"
installed_plugin="$data_home/fuzzy-mover/fuzzy-mover.so"

[[ -f "$script_dir/fuzzy-move" ]] || die "missing source file: fuzzy-move"
[[ -f "$script_dir/fuzzy-move.desktop.in" ]] || die "missing source file: fuzzy-move.desktop.in"
[[ -f "$script_dir/fuzzy-mover-set-root.desktop.in" ]] || \
    die "missing source file: fuzzy-mover-set-root.desktop.in"
[[ -f "$script_dir/fuzzy-mover-mode.c" ]] || die "missing source file: fuzzy-mover-mode.c"
[[ -f "$script_dir/fuzzy-mover-ranking.c" ]] || die "missing source file: fuzzy-mover-ranking.c"
[[ -f "$script_dir/fuzzy-mover-ranking.h" ]] || die "missing source file: fuzzy-mover-ranking.h"
command -v cc >/dev/null 2>&1 || die "a C compiler (cc) is required"
command -v pkg-config >/dev/null 2>&1 || die "pkg-config is required"
pkg-config --exists rofi glib-2.0 || \
    die "rofi development files are required (pkg-config package: rofi)"

# Desktop Entry quoting requires special handling for these characters. They
# are extraordinarily unusual in a home path, so fail clearly instead of
# generating an ambiguous Exec line.
[[ "$installed_executable" != *'"'* ]] || die 'the install path cannot contain a double quote'
[[ "$installed_executable" != *'`'* ]] || die 'the install path cannot contain a backtick'
[[ "$installed_executable" != *'$'* ]] || die 'the install path cannot contain a dollar sign'
[[ "$installed_executable" != *'\'* ]] || die 'the install path cannot contain a backslash'

temporary_dir=$(mktemp -d "${TMPDIR:-/tmp}/fuzzy-mover-install.XXXXXX")
trap 'rm -rf -- "$temporary_dir"' EXIT HUP INT TERM
temporary_desktop="$temporary_dir/fuzzy-move.desktop"
temporary_root_desktop="$temporary_dir/fuzzy-mover-set-root.desktop"
temporary_plugin="$temporary_dir/fuzzy-mover.so"

render_desktop() {
    local template="$1"
    local output="$2"
    local line

    while IFS= read -r line || [[ -n "$line" ]]; do
        case "$line" in
            'Exec=@EXECUTABLE@ %F')
                printf 'Exec="%s" %%F\n' "$installed_executable"
                ;;
            'Exec=@EXECUTABLE@ --new-s-folder %F')
                printf 'Exec="%s" --new-s-folder %%F\n' "$installed_executable"
                ;;
            'Exec=@EXECUTABLE@ --set-photos-root %f')
                printf 'Exec="%s" --set-photos-root %%f\n' "$installed_executable"
                ;;
            *)
                printf '%s\n' "$line"
                ;;
        esac
    done < "$template" > "$output"
}

render_desktop "$script_dir/fuzzy-move.desktop.in" "$temporary_desktop"
render_desktop "$script_dir/fuzzy-mover-set-root.desktop.in" "$temporary_root_desktop"

cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -fPIC -shared \
    $(pkg-config --cflags rofi) \
    "$script_dir/fuzzy-mover-mode.c" "$script_dir/fuzzy-mover-ranking.c" \
    $(pkg-config --libs glib-2.0) \
    -o "$temporary_plugin" || die "the rofi plugin could not be built"

install -Dm755 -- "$script_dir/fuzzy-move" "$installed_executable"
install -Dm755 -- "$temporary_desktop" "$installed_service"
install -Dm755 -- "$temporary_root_desktop" "$installed_root_service"
install -Dm755 -- "$temporary_plugin" "$installed_plugin"

printf 'Installed fuzzy-mover:\n'
printf '  executable:   %s\n' "$installed_executable"
printf '  Dolphin menu: %s\n' "$installed_service"
printf '  Dolphin menu: %s\n' "$installed_root_service"
printf '  rofi plugin:  %s\n' "$installed_plugin"

if ! command -v rofi >/dev/null 2>&1; then
    printf '\nWarning: rofi is not installed or is not available in PATH.\n' >&2
fi

printf '\nRestart Dolphin before assigning a keyboard shortcut.\n'
