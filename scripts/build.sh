#/bin/bash -e

cd "$( dirname "${BASH_SOURCE[0]}" )"/..

cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
