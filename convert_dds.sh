#!/bin/bash
set -e

root="$(dirname "$0")/content"

if [ "$1" = "-clean" ]; then
  echo "Removing all .dds files..."
  find "$root/packages" "$root/data" -name '*.dds' -delete
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

echo "Converting textures to DDS..."
find "$root/packages" "$root/data" -type f \( -name '*.jpg' -o -name '*.jpeg' -o -name '*.png' \) | sort | while read -r f; do
  dds="${f%.*}.dds"
  [ -f "$dds" ] && continue
  side=$(min_side "$f")
  if [ "$side" -lt 256 ]; then
    echo "  skipping (< 256px) $(basename "$f")"
    continue
  fi
  echo "  $dds"
  nvcompress -bc3 "$f" "$dds" >/dev/null 2>&1
done
echo "Done."
