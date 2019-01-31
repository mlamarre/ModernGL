FROM ubuntu:18.04 as moderngl_egl_glvnd

RUN apt-get update && apt-get install -y \
        git \
        ca-certificates \
        make \
        automake \
        autoconf \
        libtool \
        pkg-config \
        python3-pip \
        libxext-dev \
        libx11-dev \
        x11proto-gl-dev \
        libegl1-mesa-dev \
        libglvnd-dev && \
    pip3 install wheel pypicloud[server] awscli twine && \
    rm -rf /var/lib/apt/lists/*

ADD . /ModernGL
WORKDIR /ModernGL
ENTRYPOINT python3 setup.py bdist_wheel && python3 upload_to_pypi_s3.py .