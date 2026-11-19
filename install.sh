#!/bin/sh
# Install runtime packages by default; development headers are opt-in.
set -eu
cd "$(dirname "$0")"
dev=0
simulate=0
for option in "$@"; do
    case "$option" in
        --dev) dev=1 ;;
        --simulate) simulate=1 ;;
        *) echo 'Uso: ./install.sh [--dev] [--simulate]' >&2; exit 2 ;;
    esac
done
set -- apt-get install
if [ "$simulate" = 1 ]; then set -- "$@" --simulate; fi
for package in ./debs/*.deb; do
    test -f "$package" || { echo 'Nenhum pacote encontrado em debs/.' >&2; exit 1; }
    case "$package" in *-dev_*) test "$dev" = 1 || continue ;; esac
    set -- "$@" "$package"
done
if [ "$(id -u)" != 0 ] && [ "$simulate" != 1 ]; then set -- sudo "$@"; fi
exec "$@"
