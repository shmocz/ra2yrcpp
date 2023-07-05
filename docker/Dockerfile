FROM ubuntu:22.10 as mingw

RUN dpkg --add-architecture i386
RUN \
	apt-get update -y && apt-get install --no-install-recommends -y \
	bash \
	ca-certificates \
	clang-format \
	clang-tidy \
	cmake \
	cppcheck \
	g++-mingw-w64-i686-posix \
	git \
	make \
	p7zip-full

RUN \ 
	apt-get install -y \
	libz-mingw-w64-dev \
	gdb-mingw-w64 \
	gdb-mingw-w64-target \
	gdb \
	python2 \
	python3 \
	python3-pip \
	wine32

RUN pip install -U \
	cmake-format==0.6.13 \
	cpplint==1.6.1 \
	iced-x86
# Fix some library paths
RUN ln -s /usr/lib/gcc/i686-w64-mingw32/*-posix/libgcc_s_dw2-1.dll \
	/usr/lib/gcc/i686-w64-mingw32/*-posix/libstdc++-6.dll \
	/usr/i686-w64-mingw32/lib

# FIXME: rm atp stuff
# Create user and necessary folders
RUN useradd -m user && mkdir -p /home/user/project /home/user/.wine && chmod -R 0777 /home/user

RUN \
	apt-get install -y \
	gcc

# Grab protobuf sources
RUN git clone --recurse-submodules --depth 1 --branch v3.21.12 "https://github.com/protocolbuffers/protobuf.git" /app/protobuf
WORKDIR /app/protobuf
ADD toolchains/mingw-w64-i686-docker.cmake .
RUN mkdir -p target/linux64 target/win32 build/linux64 build/win32
# Build protoc (natively)
RUN cmake \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-Dprotobuf_BUILD_LIBPROTOC=OFF \
	-Dprotobuf_BUILD_PROTOC_BINARIES=ON \
	-Dprotobuf_WITH_ZLIB=OFF \
	-Dprotobuf_BUILD_TESTS=OFF \ 
	-S . \
	-B build/linux64 

RUN cmake --build build/linux64 --parallel $(nproc)
RUN cd build/linux64 && make install && make clean

# Remove GCC since it's not needed anymore
RUN apt-get remove -y gcc

# Build protobuf library DLL and protoc (win32)
# TODO: use our makefile
RUN cmake --toolchain mingw-w64-i686-docker.cmake \
	-DCMAKE_INSTALL_PREFIX=/usr/i686-w64-mingw32 \
	-Dprotobuf_BUILD_LIBPROTOC=ON \
	-Dprotobuf_BUILD_PROTOC_BINARIES=ON \
	-Dprotobuf_WITH_ZLIB=ON \
	-DProtobuf_USE_STATIC_LIBS=ON \
	-Dprotobuf_MSVC_STATIC_RUNTIME=OFF \
	-Dprotobuf_BUILD_EXAMPLES=OFF \
	-Dprotobuf_INSTALL=ON \
	-Dprotobuf_BUILD_TESTS=OFF  \
	-DZLIB_LIB=/usr/i686-w64-mingw32/lib \
	-DZLIB_INCLUDE_DIR=/usr/i686-w64-mingw32/include \
	-S . \
	-B build/win32
RUN cmake --build build/win32 --parallel $(nproc)
RUN cd build/win32 && make install && make clean
