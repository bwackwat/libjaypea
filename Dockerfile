FROM centos/systemd

EXPOSE 10000 10080 10443

RUN rpm --import /etc/pki/rpm-gpg/RPM-GPG-KEY-CentOS-7
RUN yum -y -q update
RUN yum -y -q --nogpgcheck install epel-release git
RUN yum -y -q --nogpgcheck install clang fail2ban rkhunter certbot gcc-c++ libpqxx-devel
RUN yum -y -q --nogpgcheck install libstdc++-static libstdc++ cryptopp cryptopp-devel openssl openssl-devel
RUN yum -y -q --nogpgcheck install firewalld
RUN systemctl enable fail2ban
RUN systemctl enable firewalld

RUN rkhunter --update || exit 0 # are you serious? exit 2 if updates were installed?
RUN sed -i '/\/sbin\/if/ d' /etc/rkhunter.conf
RUN rkhunter --propupd

COPY . /libjaypea

RUN ["/libjaypea/bin/setup-centos7.sh"]

RUN groupadd -r -g 1000 user && useradd -r -m -g user -u 1000 user
RUN chown -R user /libjaypea

RUN ["/libjaypea/bin/build-library.sh"]

ENTRYPOINT ["ls", "-la", "/libjaypea"]
