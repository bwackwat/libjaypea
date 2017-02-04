FROM centos:7

EXPOSE 10000 10080 10443

COPY . /libjaypea

RUN ["/libjaypea/bin/setup-centos7.sh"]

RUN groupadd -r -g 1000 user && useradd -r -m -g user -u 1000 user
RUN chown -R user /libjaypea

RUN ["/libjaypea/bin/build-library.sh"]

USER user

ENTRYPOINT ["ls -la /libjaypea"]
