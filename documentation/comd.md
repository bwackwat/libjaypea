# comd

"Communication Daemon" and "Communicator"

An experimental yet secure alternative to sshd/ssh.

Currently works! Stdin provides input line by line, but it will be much better when it is read character by character for com to be an approptiate terminal.

## Tools

* `./keyfile-gen` will generate `keyfile.new`, a valid and unique new private key and iv keyfile.
* `./comd` will start the Communication Daemon on localhost, port 3424, using `keyfile` as a keyfile.
* `./com` will attempt to connect to `comd` running on localhost, port 3424, using `keyfile` as a keyfile.
* **Use `./com -?` or `./comd -?` to run either with different settings.

## Qualities

* Relies on a private key and iv; cryptography operations via CryptoPP.
* `./bin/com` will issue remote shell commands to `./bin/comd` and print output.
* `./bin/comd` only allows one connection. It will quickly boot any conneciton which doesn't handshake or encrypt properly.
* Secure!
* TTY commands break kinda everything. e.g. I don't recommend running vim from `./com`.
