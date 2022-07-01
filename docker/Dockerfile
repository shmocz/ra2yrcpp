# Build environment for WIN32 apps
#
# VERSION               0.0.1

FROM archlinux:latest AS arch-i686-w64-mingw32
ARG MAKE_JOBS
ARG PROTOBUF_VERSION
# Update the repositories
RUN	pacman -Syu --noconfirm
RUN	pacman -S --noconfirm \
	autoconf \
	automake \
	clang \
	cmake \
	make \
	mingw-w64-binutils \
	mingw-w64-crt \
	mingw-w64-gcc \
	mingw-w64-winpthreads

FROM arch-i686-w64-mingw32 as main
# Install programs
RUN pacman -S --noconfirm \
	git \
	protobuf

RUN useradd -m user
USER user
WORKDIR /home/user

# Get protobuf
RUN git clone --depth 1 -b $PROTOBUF_VERSION --shallow-submodules --recurse-submodules https://github.com/protocolbuffers/protobuf.git
WORKDIR /home/user/protobuf
RUN ./autogen.sh && ./configure --with-protoc=/usr/bin/protoc --prefix=/home/user/i686-w64-mingw32 --host=i686-w64-mingw32
RUN make -j ${MAKE_JOBS} && make install
WORKDIR /home/user
RUN mkdir project
