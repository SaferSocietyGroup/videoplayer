FROM ubuntu:xenial
RUN apt-get -y update && apt-get -y install cmake build-essential mingw-w64 wget nasm pkg-config curl sudo

# fetch and install spank
RUN cd /tmp && wget https://github.com/noname22/spank/archive/0.9.5.tar.gz && tar xf 0.9.5.tar.gz && cd /tmp/spank-0.9.5 && ./build.sh && cp spank /usr/local/bin

# build dependencies
ADD deps /root/deps
RUN cd /root/deps && ./deps.sh all 32bit

ADD build.sh /bin/build
RUN chmod +x /bin/build
CMD build
