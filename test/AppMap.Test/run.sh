#!/bin/sh -e

cd "`dirname $0`"

export APPMAP_OUTPUT_DIR="`mktemp -td appmap.test.XXX`"
dotnet run -p ../../launcher -- test -c Release -v n

./normalize.rb "$APPMAP_OUTPUT_DIR"
diff -urN expected "$APPMAP_OUTPUT_DIR"

rm -rf "$APPMAP_OUTPUT_DIR"
