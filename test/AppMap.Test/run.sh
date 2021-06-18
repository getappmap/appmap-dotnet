#!/bin/sh -e

cd `dirname $0`

export APPMAP_OUTPUT_DIR=`mktemp -td appmap.test.XXX`
dotnet appmap test -c Release

for f in $APPMAP_OUTPUT_DIR/*.appmap.json; do
    diff -u "expected/`basename $f`" "$f"
done

rm -rf $APPMAP_OUTPUT_DIR
