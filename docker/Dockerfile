FROM ubuntu:latest as ubuntu
ARG PROTOBUF_VERSION

RUN dpkg --add-architecture i386
RUN apt-get update -y && apt-get upgrade -y

FROM ubuntu as mingw
RUN apt-get install -y \
	--no-install-recommends \
	ca-certificates \
	bash \
	cmake \
	cppcheck \
	g++-mingw-w64-i686-posix \
	git \
	jq \
	libz-mingw-w64-dev \
	make

FROM mingw as protobuf
RUN apt-get install -y \
	--no-install-recommends \
	autoconf \
	automake \
	g++ \
	libtool

WORKDIR /tmp
RUN git clone --depth 1 -b $PROTOBUF_VERSION --shallow-submodules --recurse-submodules https://github.com/protocolbuffers/protobuf.git
WORKDIR /tmp/protobuf
# Build and install the compiler
RUN ./autogen.sh && ./configure --with-protoc=/usr/bin/protoc --prefix=/usr && make -j$(nproc)
# Copy files and remove unnecessary
RUN make DESTDIR=/tmp/proto install

# Build the library
RUN make clean && ./autogen.sh && CXX=i686-w64-mingw32-g++-posix ./configure --with-protoc=/usr/bin/protoc --prefix=/usr/i686-w64-mingw32 --host=i686-w64-mingw32 && make -j$(nproc) && make DESTDIR=/tmp/proto install

FROM mingw
COPY --from=protobuf /tmp/proto/ /
RUN apt-get install -y --install-recommends wine32
RUN apt-get install -y --no-install-recommends python3 python3-pip
RUN apt-get install -y --no-install-recommends clang-tidy
RUN pip install -U iced-x86

# Free up some space
RUN apt-get -y autoremove gcc g++ && rm /usr/lib/*.a

# Fix some library paths
RUN ln -s /usr/lib/gcc/i686-w64-mingw32/10-posix/libgcc_s_dw2-1.dll \
	/usr/lib/gcc/i686-w64-mingw32/10-posix/libstdc++-6.dll \
	/usr/i686-w64-mingw32/lib

# Create user and necessary folders
RUN useradd -m user
RUN mkdir -p /home/user/project /home/user/.wine
RUN chmod -R 0777 /home/user

USER user
WORKDIR /home/user/project
