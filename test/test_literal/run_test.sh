#!/bin/sh
set -eu
DIR="$(dirname "$0")"
HSPCMP="$DIR/../../src/hspcmp/hspcmp"
COMMON="$DIR/../../common"
"$HSPCMP" -d -i -u --compath="$COMMON" "$DIR/literal.hsp"
sha256sum "$DIR/literal.ax" | awk '{print $1}' > "$DIR/tmp.sha256"
cmp "$DIR/tmp.sha256" "$DIR/expected.sha256"
rm "$DIR/tmp.sha256"
