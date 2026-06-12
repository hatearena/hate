#!/bin/bash
set -e

root="$(dirname "$0")/content"

if [ "$1" = "-clean" ]; then
  echo "Removing all .dds files..."
  find "$root/packages" "$root/data" -name '*.dds' -delete
  echo "Done."
  exit 0
fi

echo "Converting textures to DDS..."
find "$root/packages" "$root/data" -type f \( -name '*.jpg' -o -name '*.jpeg' -o -name '*.png' \) | sort | while read -r f; do
  dds="${f%.*}.dds"
  [ -f "$dds" ] && continue
  echo "  $dds"
  nvcompress -bc3 "$f" "$dds" >/dev/null 2>&1
done
echo "Done."
