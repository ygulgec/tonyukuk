#!/bin/bash
echo "Content-Type: text/plain"
echo ""
# Read POST body
read -r BODY
# Decode (basic URL decode)
CODE=$(echo "$BODY" | sed 's/kod=//;s/+/ /g;s/%0A/\n/g;s/%22/"/g;s/%7B/{/g;s/%7D/}/g;s/%3D/=/g;s/%28/(/g;s/%29/)/g;s/%2C/,/g;s/%3A/:/g')
# Write to temp file
TMPFILE=$(mktemp /tmp/play_XXXXXX.tr)
echo "$CODE" > "$TMPFILE"
OUTFILE="${TMPFILE%.tr}"
# Compile and run with timeout
if /var/www/tonyukuktr.com/derleyici/tonyukuk-derle "$TMPFILE" -o "$OUTFILE" 2>&1; then
    timeout 5 "$OUTFILE" 2>&1 || echo "[Zaman aşımı veya hata]"
fi
rm -f "$TMPFILE" "$OUTFILE" "${TMPFILE%.tr}.s" "${TMPFILE%.tr}.o"
