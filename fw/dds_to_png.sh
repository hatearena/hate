#!/bin/bash

for f in *.dds; do
    [ -e "$f" ] || continue
    magick "$f" "${f%.dds}.png"
done
