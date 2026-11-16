#!/usr/bin/env bash
set -euo pipefail

root=$(git rev-parse --show-toplevel)
temporary_dir=$(mktemp -d)
trap 'rm -rf "$temporary_dir"' EXIT
git diff --cached --name-only --diff-filter=ACMR -z > "$temporary_dir/paths"
paths=()
while IFS= read -r -d '' path; do
    paths+=("$path")
done < "$temporary_dir/paths"

needs_clang_format=false
needs_qmlformat=false
for path in "${paths[@]+${paths[@]}}"; do
    case "$path" in
        *.cpp|*.cxx|*.cc|*.h|*.hpp|*.mm)
            needs_clang_format=true
            ;;
        *.qml)
            needs_qmlformat=true
            ;;
    esac
done

if $needs_clang_format && ! command -v clang-format >/dev/null; then
    printf 'Install clang-format before committing C++ changes.\n' >&2
    exit 1
fi
if $needs_qmlformat; then
    find_qmlformat() {
        local directory=$1
        while [[ "$directory" != / ]]; do
            local candidate
            for candidate in "$directory/bin/qmlformat" "$directory/tools/Qt6/bin/qmlformat"; do
                if [[ -x "$candidate" ]]; then
                    printf '%s\n' "$candidate"
                    return 0
                fi
            done
            directory=$(dirname "$directory")
        done
        return 1
    }

    qmlformat_path="${Qt6_ROOT:+$(find_qmlformat "$Qt6_ROOT" || true)}"
    for cache in "$root"/build/*/CMakeCache.txt; do
        [[ -n "$qmlformat_path" || ! -f "$cache" ]] && continue
        qt6_dir=$(sed -n 's/^Qt6_DIR:PATH=//p' "$cache")
        qmlformat_path="${qt6_dir:+$(find_qmlformat "$qt6_dir" || true)}"
    done
    if [[ -z "$qmlformat_path" ]]; then
        qmlformat_path=$(command -v qmlformat || true)
    fi
    if [[ -z "$qmlformat_path" ]]; then
        printf "Install Qt's qmlformat before committing QML changes.\n" >&2
        exit 1
    fi
fi

index=0
for path in "${paths[@]+${paths[@]}}"; do
    case "$path" in
        *.cpp|*.cxx|*.cc|*.h|*.hpp|*.mm)
            snapshot="$temporary_dir/$index.${path##*.}"
            git show ":$path" > "$snapshot"
            clang-format --dry-run --Werror --style="file:$root/.clang-format" "$snapshot"
            ;;
        *.qml)
            snapshot="$temporary_dir/$index.qml"
            formatted="$temporary_dir/$index.formatted.qml"
            git show ":$path" > "$snapshot"
            "$qmlformat_path" --settings "$root/.qmlformat.ini" "$snapshot" > "$formatted"
            if ! cmp -s "$snapshot" "$formatted"; then
                printf 'Format staged QML before committing: %s\n' "$path" >&2
                exit 1
            fi
            ;;
    esac
    ((++index))
done
