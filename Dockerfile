FROM ubuntu
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y cmake g++ git && rm -rf /var/lib/apt/lists/*
ENV CPM_SOURCE_CACHE=/var/cache/cpm
COPY cmake /src/cmake
COPY CMakeLists.txt /src
RUN cd /src; cmake -Bbuild -DCMAKE_BUILD_TYPE=Release || :
COPY . /src
RUN mkdir /out; cd /src; cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=false; cmake --build build -j4; cp build/*.so /out/
