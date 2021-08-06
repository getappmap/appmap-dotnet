#!/bin/bash -e

cd "$( dirname "${BASH_SOURCE[0]}" )"/..

UNAME=${1-$(uname -s)}

case $UNAME in
  Linux)
    os=linux
    clrie_url=https://github.com/applandinc/CLRInstrumentationEngine/releases/download/v1.0.39-linux/libInstrumentationEngine.so.gz
    sosuffix=so
    ;;
  Darwin)
    os=osx
    clrie_url=https://github.com/applandinc/CLRInstrumentationEngine/releases/download/v1.0.39-macos/libInstrumentationEngine.dylib.gz
    sosuffix=dylib
    ;;
  *) echo "unknown OS"; exit 1;;
esac
clrie=libInstrumentationEngine.$sosuffix

dir=bin/$os-x64
mkdir -p $dir

path=$dir/$clrie

[ -f $path ] || curl -L $clrie_url | gunzip > $path
