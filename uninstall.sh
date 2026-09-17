#!/usr/bin/env bash

set -eu

usage() {
    cat <<'EOF'
Usage: ./uninstall.sh [--purge-config]

Removes fuzzy-mover from the current user's Dolphin installation.
The Photos root configuration is preserved unless --purge-config is used.

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
config_file="$config_home/fuzzy-mover/photos-root"

rm -f -- "$installed_executable" "$installed_service"

printf 'Removed:\n'
printf '  %s\n' "$installed_executable"
printf '  %s\n' "$installed_service"

if [[ "$purge_config" == true ]]; then
    rm -f -- "$config_file"
    rmdir -- "$config_home/fuzzy-mover" 2>/dev/null || true
    printf '  %s\n' "$config_file"
else
    printf '\nConfiguration preserved: %s\n' "$config_file"
fi

printf '\nRestart Dolphin to remove the action and its shortcut entry.\n'
