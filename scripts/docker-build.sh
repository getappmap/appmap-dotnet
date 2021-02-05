#/bin/bash -e

BASEDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && cd .. && pwd )"

docker build ${BASEDIR}

mkdir -p out
image=$(docker build -q $BASEDIR)
id=$(docker create $image)
docker cp $id:/out - | tar xvC $BASEDIR
docker rm -v $id > /dev/null
