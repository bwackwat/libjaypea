#!/usr/bin/python3

import subprocess, os, sys, time, atexit, signal, psutil, datetime

if len(sys.argv) < 3:
	print("Usage: watcher.py <directories and/or files to watch, comma separated> <command to terminate and repeat> <optional \"forever\">")
	sys.exit(1)

forever = False
if len(sys.argv) > 3 and sys.argv[3] == "forever":
	forever = True

watch = sys.argv[1].split(",")

print("COMMAND: " + sys.argv[2])
process = subprocess.Popen(sys.argv[2], shell=True, stdout=sys.stdout, stderr=sys.stderr)
done = False

# "&&" within the command spawns children. Must vanquish all.
def stop_process():
	if done:
		return
	temp = psutil.Process(process.pid)
	for proc in temp.children(recursive=True):
		proc.kill()
	temp.kill()
	
atexit.register(stop_process)

filetime = {}

def file_changed(file):
	mtime = os.stat(file).st_mtime
	if not file in filetime:
		filetime[file] = mtime
	elif filetime[file] != mtime:
		filetime[file] = mtime
		print("CHANGE" + " (" + str(datetime.datetime.now()) + "): " + file)
		return True
	return False
	
def dir_changed(dir):
	for file in os.listdir(dir):
		if os.path.isfile(dir + file):
			if not file.endswith(".swp") and file_changed(dir + file):
				return True
		elif os.path.isdir(dir + file) and file != "index":
			if dir_changed(dir + file):
				return True
	return False
	
def any_changed():
	for item in watch:
		if os.path.isfile(item):
			if file_changed(item):
				return True
		elif os.path.isdir(item):
			if dir_changed(item):
				return True
		else:
			filetime[item] = 0
	return False

print("WATCHING FOR CHANGES (" + str(datetime.datetime.now()) + "): " + sys.argv[1])

def restart():
	global process
	if process:
		stop_process()
	print("COMMAND: " + sys.argv[2])
	process = subprocess.Popen(sys.argv[2], shell=True, stdout=sys.stdout, stderr=sys.stderr)

while True:
	changed = False
	while not changed:
		if process and process.poll() is not None and not done:
			if forever:
				process = None
				restart()
			else:
				done = True
				print("DONE" + " (" + str(datetime.datetime.now()) + "), WATCHING FOR CHANGES: " + sys.argv[1])
		time.sleep(1)
		if any_changed():
			restart()
			done = False

