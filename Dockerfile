FROM centos:7

ADD . /libjaypea

CMD ["/libjaypea/bin/setup-centos7.sh"]
CMD ["/libjaypea/bin/build-library.sh"]
