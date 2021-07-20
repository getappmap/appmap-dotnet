#/bin/bash -e

BASEDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && cd .. && pwd )"
mkdir -p $BASEDIR/out

CLRIE_VERSION=1.0.36
CLRIE_TARBALL=instrumentationengine-release-${CLRIE_VERSION}.tar.gz
CLRIE_DIR=CLRInstrumentationEngine-instrumentationengine-release-${CLRIE_VERSION}

build_clrie() {
  version="$1"
  if ! [ -d $CLRIE_DIR ]; then
    curl -L https://github.com/microsoft/CLRInstrumentationEngine/archive/$CLRIE_TARBALL | tar zxv
  fi

  pushd $CLRIE_DIR
  docker build -t clrie-build-ubuntu src/unix/docker/dockerfiles/build/ubuntu/
  sh src/unix/docker/docker-build.sh clrie-build-ubuntu x64 Release
  cp out/Linux/bin/x64.Release/ClrInstrumentationEngine/* $BASEDIR/out/
}

build_appmap() {
  docker build ${BASEDIR}

  image=$(docker build -q $BASEDIR)
  id=$(docker create $image)
  docker cp "$id:/src/build/libappmap-instrumentation.so" $BASEDIR/out
  docker rm -v $id > /dev/null
}

[ -f $BASEDIR/out/libInstrumentationEngine.so ] || build_clrie
build_appmap
cp config/* out
dotnet pack -c release launcher
