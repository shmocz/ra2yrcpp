FROM ubuntu:22.04 as mingw
ARG PROTOBUF_VERSION

RUN apt-get update
RUN apt-get install -y \
	autoconf \
	automake \
	bash \
	cmake \
	g++ \
	g++-mingw-w64-i686 \
	g++-mingw-w64-i686-posix \
	git \
	libtool \
	make

FROM mingw as protobuf
RUN useradd -m user
USER user
WORKDIR /home/user

RUN git clone --depth 1 -b $PROTOBUF_VERSION --shallow-submodules --recurse-submodules https://github.com/protocolbuffers/protobuf.git
WORKDIR /home/user/protobuf
# Build and install the compiler
RUN ./autogen.sh && ./configure --with-protoc=/usr/bin/protoc --prefix=/usr && make -j$(nproc)
USER root
RUN make install
USER user
# Build the library and remove intermediate files
RUN make clean && \
	./autogen.sh && \
	CXX=i686-w64-mingw32-g++-posix ./configure --with-protoc=/usr/bin/protoc --prefix=/home/user/i686-w64-mingw32 --host=i686-w64-mingw32 && \
	make -j$(nproc) && \
	make install && \
	make clean
RUN mkdir /home/user/project
WORKDIR /home/user/project
