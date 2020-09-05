FROM ubuntu:18.04

RUN apt-get update && apt-get -y upgrade && \
    apt-get -y install apt-utils gcc automake zlib1g-dev make

RUN apt-get -y install g++ git

RUN DEBIAN_FRONTEND=noninteractivei apt-get -y install build-essential software-properties-common

# Get the solver, as it has the relevant scripts
ADD . /opt/mergesat

# Modify docker glibc

# Install modified glibc 2.23
# Get dependencies
RUN apt-get -y install fakeroot dpkg-dev \
    systemtap-sdt-dev \
    gawk \
    perl \
    autoconf \
    gettext \
    bison \
    wget \
    texi2html \
    texinfo

# Relevant for interaction with aws
RUN apt-get install -y unzip
RUN DEBIAN_FRONTEND=noninteractive apt install -y iproute2 cmake python python-pip gfortran curl
RUN pip install supervisor awscli

# Get glibc, patch, build, and install (build with default gcc)
RUN /opt/mergesat/tools/inject-thp-glibc.sh install

# Get a more recent gcc
#RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y
#RUN apt-get update -q
#RUN apt-get remove -y gcc g++
#RUN DEBIAN_FRONTEND=noninteractive apt-get install g++-9 gcc-9 -y
#RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-9
#RUN update-alternatives --install /usr/bin/gcc-ar gcc-ar /usr/bin/gcc-ar-9 60
#RUN update-alternatives --install /usr/bin/gcc-ranlib gcc-ranlib /usr/bin/gcc-ranlib-9 60
#RUN update-alternatives --install /usr/bin/x86_64-linux-gnu-gcc x86_64-linux-gnu-gcc /usr/bin/x86_64-linux-gnu-gcc-9 60 --slave /usr/bin/x86_64-linux-gnu-g++ x86_64-linux-gnu-g++ /usr/bin/x86_64-linux-gnu-g++-9

# Build the solver
ADD . /opt/mergesat
RUN ls /opt/mergesat

# Clean solver
RUN cd /opt/mergesat && make clean
RUN rm -rf /opt/mergesat/build

# Build solver from scratch
RUN cd /opt/mergesat && make r

# Enable THB via environment?
ENV GLIBC_THP_ALWAYS=1

# Check whether the binary is statically linked
RUN file /opt/mergesat/build/release/bin/mergesat

# Run the solver with the environment variable parameters
CMD /opt/mergesat/tools/container-default-command.sh
