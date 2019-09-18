# Ponal

A TCP server and client for simple data communication and preservation.

Inspired by Redis.

## Purpose

I was having a bit of trouble finding a robust and simple way to configure an array of applications. I decided to forge my own IP-based tool to do so.

The Ponal daemon (ponald) is meant to stay alive on a private network server and communicate with Ponal clients (ponal).

Currently, Ponal is meant to be an easy configuration tool for bash and C++;

## Commands

All commands return "failure" on any kind of failure.

All commands return "success" when there is no return value.

All commands are strictly lowercase-sensitive.

### Get

Usage: "get [key]"

Note: Keys cannot contain spaces.

### Set

Usage: "set [key] [value]"

Note: Values can contain any characters, keys can contain anything but spaces.

### Exit

Usage: "exit"

## Example

```bash
./artifacts/ponal << EOF
get test
set test apples
set test
get test
exit
EOF
```

```
get test >> test has not been set yet >> failure
set test apples >> test is set to apples >> success
set test >> no value provided >> failure
get test >> test was set to apples >> apples
exit >> Bye!
```

Expect:

```bash
> failure
> success
> failure
> apples
> Bye!
```

## Complete Example

```bash
#!/bin/bash

./scripts/build-library.sh

./scripts/build-example.sh

./artifacts/ponald --port 12345 --quiet &

# Let ponald start.
sleep 1

./artifacts/ponal --port 12345 <<< "set connection_string dbname=test user=postgres password=aq12ws"

echo

connection_string="$(./artifacts/ponal --port 12345 <<< 'get connection_string')"

echo "${connection_string}"
```
