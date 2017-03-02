$script = <<SCRIPT
/vagrant/scripts/deploy.sh /vagrant "A symmetrically encrypted epoll server from libjaypea can use any text like this basically for key file." deploy
SCRIPT

Vagrant.configure("2") do |config|
  config.vm.define "devel", autostart: true, primary: true do |devel|
    devel.vm.box = "bento/centos-7.2"
    devel.vm.provider "virtualbox" do |vb|
      vb.customize ["modifyvm", :id, "--memory", "1024"]
    end
  end
  config.vm.network "private_network", type: "dhcp"
  config.vm.provision "shell", inline: $script
end
