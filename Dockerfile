FROM proddiagbuild.azurecr.io/clrie-build-ubuntu:latest AS clrie
ENV CLRIE_VERSION=1.0.36
RUN curl -L https://github.com/microsoft/CLRInstrumentationEngine/archive/instrumentationengine-release-${CLRIE_VERSION}.tar.gz | tar zxv
RUN cd CLRInstrumentationEngine-instrumentationengine-release-${CLRIE_VERSION}/src; ./build.sh

FROM ubuntu
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y cmake g++ git && rm -rf /var/lib/apt/lists/*
COPY cmake /src/cmake
COPY CMakeLists.txt /src
RUN cd /src; cmake -Bbuild -DCMAKE_BUILD_TYPE=Release || :
COPY . /src
RUN cd /src; cmake -Bbuild -DCMAKE_BUILD_TYPE=Release; cmake --build build -j4
