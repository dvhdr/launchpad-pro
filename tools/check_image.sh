#!/bin/sh
# Usage: check_image.sh ELF
# Verifies the image fits below the end of the app flash region, including the
# last 35-byte block hextosyx reads (blocks start at base + 32*k while < end).
set -e
ELF=$1
LIMIT=$((0x0801F800))
BLOCK=32
READ=35

[ -n "$ELF" ] || { echo "usage: $0 ELF" >&2; exit 2; }

# base = lowest LMA, end = max(LMA + size) over ALLOC+LOAD sections.
# Parsed in the shell: objdump -h prints a data line (Idx Name Size VMA LMA ...)
# followed by a flags line; plain awk has no hex parsing.
BASE=
END=0
SIZE=
LMA=
while read -r f1 f2 f3 f4 f5 rest; do
	case "$f1" in
	[0-9]*)
		SIZE=$((0x$f3)); LMA=$((0x$f5)) ;;
	*)
		# flags line; only LOAD sections occupy flash (.bss is ALLOC only)
		case "$f1 $f2 $f3 $f4 $f5 $rest" in
		*ALLOC*LOAD*)
			if [ "$SIZE" -gt 0 ]; then
				if [ -z "$BASE" ] || [ "$LMA" -lt "$BASE" ]; then BASE=$LMA; fi
				if [ $((LMA + SIZE)) -gt "$END" ]; then END=$((LMA + SIZE)); fi
			fi ;;
		esac ;;
	esac
done <<EOT
$(arm-none-eabi-objdump -h "$ELF")
EOT
[ -n "$BASE" ] || { echo "ERROR: no ALLOC sections in $ELF" >&2; exit 1; }

# last block start: base + 32*k, largest value < end
LAST=$((BASE + ((END - BASE - 1) / BLOCK) * BLOCK))
LASTEND=$((LAST + READ))

printf 'image end   0x%08X\n' "$END"
printf 'last block  0x%08X (reads to 0x%08X)\n' "$LAST" "$LASTEND"
printf 'remaining   %d bytes\n' "$((LIMIT - LASTEND))"

if [ "$END" -gt "$LIMIT" ]; then
	echo "ERROR: image end exceeds 0x0801F800" >&2
	exit 1
fi
if [ "$LASTEND" -gt "$LIMIT" ]; then
	echo "ERROR: last hextosyx block reads past 0x0801F800" >&2
	exit 1
fi
