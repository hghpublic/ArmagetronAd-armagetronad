ARG BASE_ALPINE=amd64/alpine:3.12
ARG CONFIGURE_ARGS=""
ARG FAKERELEASE=false

########################################

# development prerequisites
FROM ${BASE_ALPINE} AS builder
LABEL maintainer="Manuel Moos <z-man@users.sf.net>"

RUN mkdir src && wget https://forums3.armagetronad.net/download/file.php?id=9628 -O src/zthread.patch.bz2
RUN wget https://sourceforge.net/projects/zthread/files/ZThread/2.3.2/ZThread-2.3.2.tar.gz/download -O src/zthread.tgz

# build dependencies
RUN apk add \
autoconf \
automake \
patch \
bash \
bison \
g++ \
make \
libxml2-dev \
python3 \
--no-cache

# for 0.4
#protobuf-dev \
#boost-dev \
#boost-thread \

RUN cd src && tar -xzf zthread.tgz && cd ZThread* && bzcat ../zthread.patch.bz2 | patch -p 1
RUN cd src/ZThread* && CXXFLAGS="-fpermissive -DPTHREAD_MUTEX_RECURSIVE_NP=PTHREAD_MUTEX_RECURSIVE" ./configure --prefix=/usr --enable-shared=yes --enable-static=no && \
make -j `nproc` && \
make install

#RUN find /usr/lib/ -name *ZThread*

########################################

# build
FROM builder as build

ENV SOURCE_DIR /root/armagetronad
ENV BUILD_DIR /root/build

ARG CONFIGURE_ARGS
ARG FAKERELEASE
ARG BRANCH

COPY . ${SOURCE_DIR}
WORKDIR ${SOURCE_DIR}

RUN (test -r configure && test -f missing) || (./bootstrap.sh && cat version.m4)

RUN mkdir -p ${BUILD_DIR} && chmod 755 ${BUILD_DIR}
WORKDIR ${BUILD_DIR}
RUN ARMAGETRONAD_FAKERELEASE=${FAKERELEASE} ../armagetronad/configure --prefix=/usr/local --disable-glout --disable-sysinstall --disable-useradd --disable-master --disable-uninstall --disable-desktop ${CONFIGURE_ARGS}
RUN make -j `nproc`
RUN DESTDIR=/root/destdir make install

########################################

# runtime prerequisites
FROM ${BASE_ALPINE} AS runtime
LABEL maintainer="Manuel Moos <z-man@users.sf.net>"

RUN apk add \
bash \
libxml2-dev \
libgcc \
libstdc++ \
shadow \
--no-cache

# for 0.4
# boost-thread \
# protobuf \

COPY --chown=root --from=builder /usr/lib/*ZThread*.so* /usr/lib/
#RUN find /usr/lib/ -name *ZThread*

WORKDIR /
RUN adduser -D armagetronad

########################################

# pack
FROM runtime AS run_server
MAINTAINER Manuel Moos <z-man@users.sf.net>

COPY --chown=root --from=build /root/destdir /
RUN sh /usr/local/share/games/*-dedicated/scripts/sysinstall install /usr/local

USER armagetronad

ENTRYPOINT /usr/local/bin/armagetronad-dedicated
EXPOSE 4534/udp
