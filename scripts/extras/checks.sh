#!/bin/bash

# https://www.cyberciti.biz/tips/linux-security.html

# All SUID/SGID bits enabled file can be misused when the SUID/SGID executable has a security problem or bug.
# All local or remote user can use such file.
# It is a good idea to find all such files. Use the find command as follows:
# You need to investigate each reported file. See reported file man page for further details.

# See all set user id files:
find / -perm +4000

# See all group id files
find / -perm +2000

# Or combine both in a single command
find / \( -perm -4000 -o -perm -2000 \) -print
find / -path -prune -o -type f -perm +6000 -ls

# Anyone can modify world-writable file resulting into a security issue. 
# Use the following command to find all world writable and sticky bits set files:
# You need to investigate each reported file and either set correct user and group permission or remove it.
find / -xdev -type d \( -perm -0002 -a ! -perm -1000 \) -print

# Files not owned by any user or group can pose a security problem.
# Just find them with the following command which do not belong to a valid user and a valid group.
find / -xdev \( -nouser -o -nogroup \) -print

