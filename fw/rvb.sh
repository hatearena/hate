find . -type f -iname '*.wav' | while read -r f; do
    tmp="${f%.wav}.tmp.wav"

    sox "$f" "$tmp" reverb 10 40 20
    mv "$tmp" "$f"
done
