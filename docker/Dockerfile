
# to build and test:
# docker build -t altra64-dev . && docker run --rm -v "$(pwd):/build" -it altra64-dev

# to use to compile altra64 (or other n64 stuff I guess)
# docker run --rm -v "$(pwd):/build" altra64-dev make

#FROM ubuntu:18.04
FROM ubuntu:16.04

ARG INSTALL_DEPS="wget build-essential git texinfo libc6 libgmp-dev libmpfr-dev libmpc-dev libpng-dev zlib1g-dev libtool autoconf"

RUN apt-get update \
    && apt-get -y upgrade \ 
    && apt-get -y install ${INSTALL_DEPS} \
    && apt-get -y autoremove \
    && apt-get autoclean

ENV N64_INST=/usr/local/libdragon
ENV PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/local/libdragon/bin

RUN mkdir /build

COPY setup-docker.sh /usr/bin/setup-docker.sh

# run it then delete all build artifacts
RUN /usr/bin/setup-docker.sh && rm -rf /root/*

VOLUME [ "/build" ]

WORKDIR /build

CMD ["bash"]
