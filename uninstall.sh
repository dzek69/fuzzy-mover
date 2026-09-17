#!/usr/bin/env bash

set -eu

usage() {
    cat <<'EOF'
Usage: ./uninstall.sh [--purge-config]

Removes fuzzy-mover from the current user's Dolphin installation.
Configuration is preserved unless --purge-config is used.

Optional environment variables:
  FUZZY_MOVER_BIN_DIR      Executable directory (default: ~/.local/bin)
  XDG_DATA_HOME            XDG data directory (default: ~/.local/share)
  XDG_CONFIG_HOME          XDG config directory (default: ~/.config)
EOF
}

purge_config=false

case "${1:-}" in
    "") ;;
    --purge-config) purge_config=true ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

bin_dir="${FUZZY_MOVER_BIN_DIR:-$HOME/.local/bin}"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
installed_executable="$bin_dir/fuzzy-move"
installed_service="$data_home/kio/servicemenus/fuzzy-move.desktop"
installed_plugin="$data_home/fuzzy-mover/fuzzy-mover.so"
config_file="$config_home/fuzzy-mover/photos-root"
theme_config_file="$config_home/fuzzy-mover/rofi-theme"
exclude_config_file="$config_home/fuzzy-mover/exclude-regexes"

rm -f -- "$installed_executable" "$installed_service" "$installed_plugin"
rmdir -- "$data_home/fuzzy-mover" 2>/dev/null || true

printf 'Removed:\n'
printf '  %s\n' "$installed_executable"
printf '  %s\n' "$installed_service"
printf '  %s\n' "$installed_plugin"

if [[ "$purge_config" == true ]]; then
    rm -f -- "$config_file" "$theme_config_file" "$exclude_config_file"
    rmdir -- "$config_home/fuzzy-mover" 2>/dev/null || true
    printf '  %s\n' "$config_file"
    printf '  %s\n' "$theme_config_file"
    printf '  %s\n' "$exclude_config_file"
else
    printf '\nConfiguration preserved under: %s\n' "$config_home/fuzzy-mover"
fi

printf '\nRestart Dolphin to remove the action and its shortcut entry.\n'
