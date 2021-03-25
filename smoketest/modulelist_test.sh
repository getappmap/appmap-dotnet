#!/bin/sh -e

BASEDIR=$(realpath $(dirname "$0")/..)
cd $BASEDIR/smoketest/hello
dotnet build -o bin

export APPMAP_LIST_MODULES=`mktemp`
trap "rm $APPMAP_LIST_MODULES" 0
$BASEDIR/scripts/dotnet-appmap $BASEDIR/smoketest/hello/bin/hello
cat $APPMAP_LIST_MODULES
grep -q hello.dll $APPMAP_LIST_MODULES
