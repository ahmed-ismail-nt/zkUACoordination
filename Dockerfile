FROM debian:latest
RUN apt-get update && apt-get install -y cmake gcc g++ musl-dev python make libzookeeper-mt-dev libjansson-dev autotools-dev dh-autoreconf && apt-get autoclean
ADD . /var/tmp/zkUA
WORKDIR /var/tmp/zkUA
RUN ACLOCAL="aclocal -I /usr/share/aclocal" autoreconf -if && ./configure && make && make install
RUN echo "/usr/local/lib" >> /etc/ld.so.conf.d/randomLibs.conf && ldconfig
