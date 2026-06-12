#!/bin/bash
set -e

has_alpha() {
  local f="$1"
  case "$f" in
    *.jpg|*.jpeg) return 1 ;;
    *.png)
      if command -v identify &>/dev/null; then
        channels=$(identify -format '%[channels]' "$f" 2>/dev/null)
        [[ "$channels" == *"a"* ]] && return 0 || return 1
      elif python3 -c "from PIL import Image; import sys; im=Image.open(sys.argv[1]); sys.exit(0 if 'A' in im.mode else 1)" "$f" 2>/dev/null; then
        return 0
      fi
      return 1 ;;
    *) return 1 ;;
  esac
}

root="$(dirname "$0")/content"

echo "Converting textures to DDS..."
find "$root/packages" "$root/data" -type f \( -name '*.jpg' -o -name '*.jpeg' -o -name '*.png' \) | sort | while read -r f; do
  dds="${f%.*}.dds"
  [ -f "$dds" ] && continue
  if has_alpha "$f"; then
    echo "  $dds (DXT5)"
    nvcompress -bc3 -alpha "$f" "$dds" >/dev/null 2>&1
  else
    echo "  $dds (DXT1)"
    nvcompress -bc1 "$f" "$dds" >/dev/null 2>&1
  fi
done
echo "Done."
