#!/bin/sh -e

BASEDIR=$(realpath $(dirname "$0")/..)
cd $BASEDIR/smoketest/hello

export APPMAP_OUTPUT_PATH=`mktemp`
export APPMAP_CLASSMAP=true
trap "rm $APPMAP_OUTPUT_PATH" 0
$BASEDIR/scripts/run.sh $BASEDIR/smoketest/hello/bin/hello
jq . $APPMAP_OUTPUT_PATH
jq . $APPMAP_OUTPUT_PATH | diff - $BASEDIR/smoketest/expected.appmap.json
