#/bin/bash -e

cd "$( dirname "${BASH_SOURCE[0]}" )"/..

case "$(uname -s)" in
  Linux)
    os=linux
    sosuffix=so
    ;;
  Darwin)
    os=osx
    sosuffix=dylib
    ;;
  *) echo "unknown OS"; exit 1;;
esac

# assumes build is completed

# run unit tests

build/test/appmap-test

# download CLRIE, run integration tests

scripts/get-clrie.sh

export APPMAP_RUNTIME_DIR=$PWD/bin/$os-x64

cp -f build/*.$sosuffix $APPMAP_RUNTIME_DIR
cp -f config/ProductionBreakpoints_x64.$os.config $APPMAP_RUNTIME_DIR/ProductionBreakpoints_x64.config

APPMAP_LOG_LEVEL=debug test/AppMap.Test/run.sh
