FROM ubuntu:trusty
RUN apt-get -y update && apt-get -y install git cmake build-essential mingw-w64 wget nasm pkg-config

ADD build.sh /bin/build
RUN chmod +x /bin/build
CMD build
