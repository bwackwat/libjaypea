FROM centos:7

RUN mkdir -p /opt/libjaypea

COPY . /opt/libjaypea

WORKDIR /opt/libjaypea

RUN ./scripts/setup-centos7.sh

RUN ./scripts/build-library.sh PROD

RUN ./scripts/build-example.sh
