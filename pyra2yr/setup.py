import re
import os
import glob
import shutil
from setuptools import setup, find_packages

PKG_PYRA2YR = "pyra2yr"
PKG_RA2YRPROTO = "ra2yrproto"
PROTO_SOURCES = os.environ.get("PROTO_SOURCES", None) or os.path.join(
    "build", "src", "protocol", PKG_RA2YRPROTO
)


# setup compiled protobuf files
# TODO: check that these are generated correctly
proto_files = [
    os.path.join(PROTO_SOURCES, f)
    for f in os.listdir(PROTO_SOURCES)
    if re.match(r".+\.(py|pyi)$", f)
]

for src in proto_files:
    dest = os.path.join(PKG_RA2YRPROTO, os.path.basename(src))
    if (
        not os.path.exists(dest)
        or os.stat(dest).st_mtime < os.stat(src).st_mtime
    ):
        shutil.copyfile(src, dest)
        print(f"{src} -> {dest}")


setup(
    name=PKG_PYRA2YR,
    version="0.0.1",
    description="RA2YR Python interface",
    url="",
    license="MIT",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "License :: OSI Approved :: MIT License",
    ],
    packages=find_packages(),
    data_files=[
        (PKG_RA2YRPROTO, glob.glob(os.path.join(PKG_RA2YRPROTO, "*.pyi")))
    ],
    package_dir={PKG_PYRA2YR: PKG_PYRA2YR, PKG_RA2YRPROTO: PKG_RA2YRPROTO},
    entry_points={
        "console_scripts": ["{n}={n}.main:main".format(n=PKG_PYRA2YR)]
    },
)
