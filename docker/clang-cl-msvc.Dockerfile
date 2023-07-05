FROM ubuntu:23.04 as msvc-wine

RUN apt-get update && \
    apt-get install -y wine64-development python3 msitools python3-simplejson git \
    python3-six ca-certificates && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir /opt/msvc

# Clone mstorsjo's repo
RUN git clone "https://github.com/mstorsjo/msvc-wine.git" /opt/msvc-wine

WORKDIR /opt/msvc-wine

RUN PYTHONUNBUFFERED=1 ./vsdownload.py --accept-license --dest /opt/msvc && \
    ./install.sh /opt/msvc && \
    rm lowercase fixinclude install.sh vsdownload.py && \
    rm -rf wrappers

FROM msvc-wine as clang-cl

RUN dpkg --add-architecture i386

RUN --mount=type=cache,target=/var/cache/apt \
    apt-get update && \
    apt-get install -y \ 
    clang-15  \
    clang-tools-15 \
    cmake \
    libz-mingw-w64-dev \
    lld-15 \
    make \
    ninja-build \
    wine32-development

RUN  for p in lld-link clang-cl llvm-rc llvm-lib; do \
    ln -s /usr/bin/$p-15 /usr/bin/$p; \
    done

# TODO: probably not needed
RUN ln -s /usr/bin/llvm-rc /usr/bin/rc
# Initialize wineroot
RUN wine64 wineboot --init && \
    while pgrep wineserver > /dev/null; do sleep 1; done

RUN --mount=type=cache,target=/var/cache/apt \
    apt-get install -y --no-install-recommends python3 python3-pip && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* && \
    pip install --break-system-packages -U iced-x86

# Remove unused stuff
RUN rm -rf /opt/msvc/VC/Tools/MSVC/*/lib/{arm,arm64,arm64ec} \
    /opt/msvc/kits/10/Lib/*/{ucrt,um}/{arm,arm64} \
    /opt/msvc/kits/10/Redist/*/{arm,arm64} \
    /opt/msvc/kits/10/Redist/*/ucrt/DLLs/{arm,arm64} \
    /opt/msvc/kits/10/bin/{arm,arm64} \
    /opt/msvc/bin/{arm,arm64}

RUN useradd -m user && mkdir -p /home/user/project /home/user/.wine && chmod -R 0777 /home/user
