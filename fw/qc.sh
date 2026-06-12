#!/bin/bash
set -euo pipefail

root="${1:-.}"

find "$root" -type f \( -iname '*.jpg' -o -iname '*.jpeg' \) | while read -r jpg; do
    dds="${jpg%.*}.dds"

    echo "$jpg -> $dds"

    nvcompress -rgb "$jpg" "$dds"
done
