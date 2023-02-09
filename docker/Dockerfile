FROM ubuntu:22.10 as mingw

RUN dpkg --add-architecture i386
RUN --mount=type=cache,target=/var/cache/apt \
	apt-get update -y && apt-get install --no-install-recommends -y \
	bash \
	ca-certificates \
	cmake \
	cppcheck \
	g++-mingw-w64-i686-posix \
	git \
	clang-tidy \
	make

RUN --mount=type=cache,target=/var/cache/apt \
	apt-get install -y \
	libz-mingw-w64-dev \
	gdb-mingw-w64 \
	gdb-mingw-w64-target \
	gdb \
	python2 \
	python3 \
	python3-pip \
	wine32

RUN pip install -U iced-x86
# Fix some library paths
RUN ln -s /usr/lib/gcc/i686-w64-mingw32/*-posix/libgcc_s_dw2-1.dll \
	/usr/lib/gcc/i686-w64-mingw32/*-posix/libstdc++-6.dll \
	/usr/i686-w64-mingw32/lib

# FIXME: rm atp stuff
# Create user and necessary folders
RUN useradd -m user && mkdir -p /home/user/project /home/user/.wine && chmod -R 0777 /home/user