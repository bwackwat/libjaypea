import subprocess, os, sys, time, atexit, signal, psutil

if len(sys.argv) < 3:
	print "Usage: watcher.py <directories and/or files to watch, comma separated> <command to terminate and repeat>"
	sys.exit(1)

print "------------------------------------------------------------------"

watch = sys.argv[1].split(",")
directories = []
files = []
for item in watch:
	if os.path.isdir(item):
		directories.append(item)
	elif os.path.isfile(item):
		files.append(item)
	else:
		print "BAD DIRECTORY OR FILE: " + item

print "DIRECTORIES: " + ", ".join(directories)
print "FILES: " + ", ".join(files)
print "COMMAND: " + sys.argv[2]

print "------------------------------------------------------------------"

def start_process():
	return subprocess.Popen(sys.argv[2], shell=True, stdout=sys.stdout, stderr=sys.stderr)

process = start_process()

# "&&" within the command spawns children. Must vanquish all.
def stop_process():
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
	else:
		if filetime[file] != mtime:
			filetime[file] = mtime
			print "------------------------------------------------------------------"
			print "CHANGE: " + file
			print "RESTARTING"
			print "------------------------------------------------------------------"
			return True
	return False

def dir_changed(dir):
	for file in os.listdir(dir):
		if not os.path.isfile(dir + file):
			continue
		if not file.endswith(".swp") and file_changed(dir + file):
			return True
	return False

def any_changed():
	for dir in directories:
		if dir_changed(dir):
			return True
	for file in files:
		if file_changed(file):
			return True
	return False

done = False;
while True:
	changed = False
	while not changed:
		if process.poll() is not None and not done:
			done = True
			print "DONE!"
		time.sleep(1)
		if any_changed():
			if not done:
				stop_process()
			process = start_process()
			done = False
