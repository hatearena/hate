#!/bin/bash
set -e

cd "$(dirname "$0")/.."

root="content"

dry=0
prune=0
clean=0

for arg in "$@"; do
  case "$arg" in
    -dry)   dry=1 ;;
    -prune) prune=1 ;;
    -clean) clean=1 ;;
    *)
      echo "Unknown option: $arg"
      exit 1
      ;;
  esac
done

if [ "$clean" -eq 1 ]; then
  echo "Removing all .dds files..."
  find "$root/packages" "$root/data" -name '*.dds' -delete
  echo "Done."
  exit 0
fi

if [ "$prune" -eq 1 ]; then
  if [ "$dry" -eq 1 ]; then
    echo "Dry: Source images that would be removed..."
  else
    echo "Removing source images that have a DDS counterpart..."
  fi

  find "$root/packages" "$root/data" -name '*.dds' | while read -r dds; do
    base="${dds%.*}"
    for ext in jpg jpeg png; do
      src="${base}.${ext}"
      if [ -f "$src" ]; then
        if [ "$dry" -eq 1 ]; then
          echo "  would prune $src"
        else
          echo "  pruning $src"
          rm "$src"
        fi
      fi
    done
  done

  echo "Done."
  exit 0
fi

min_side() {
  local f="$1"
  if command -v identify &>/dev/null; then
    set -- $(identify -format '%w %h' "$f" 2>/dev/null)
    echo "$(( $1 < $2 ? $1 : $2 ))"
  elif python3 -c "
from PIL import Image; import sys
im = Image.open(sys.argv[1])
print(min(im.size))
" "$f" 2>/dev/null; then
    :
  else
    echo 999
  fi
}

if [ "$dry" -eq 1 ]; then
  echo "Dry: Images that would be converted to DDS..."
else
  echo "Converting textures to DDS..."
fi

find "$root/packages" "$root/data" -type f \( -name '*.jpg' -o -name '*.jpeg' -o -name '*.png' \) | sort | while read -r f; do
  dds="${f%.*}.dds"

  [ -f "$dds" ] && continue

  side=$(min_side "$f")
  if [ "$side" -lt 256 ]; then
    echo "  skipping (< 256px) $(basename "$f")"
    continue
  fi

  if [ "$dry" -eq 1 ]; then
    echo "  would convert $f -> $dds"
  else
    echo "  $dds"
    nvcompress -rgb "$f" "$dds" >/dev/null 2>&1
  fi
done

echo "Done."
