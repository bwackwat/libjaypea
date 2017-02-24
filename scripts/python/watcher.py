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

process = None

# "&&" within the command spawns children. Must vanquish all.
def stop_process():
	temp = psutil.Process(process.pid)
	for proc in temp.children(recursive=True):
		proc.kill()
	temp.kill()
	
atexit.register(stop_process)

filetime = {}
def any_changed():
	def file_changed(file):
		mtime = os.stat(file).st_mtime
		if not file in filetime:
			filetime[file] = mtime
		else:
			if filetime[file] != mtime:
				filetime[file] = mtime
				print "CHANGE: " + file
				return True
		return False
	def dir_changed(dir):
		for file in os.listdir(dir):
			if not os.path.isfile(dir + file):
				continue
			if not file.endswith(".swp") and file_changed(dir + file):
				return True
		return False
	for dir in directories:
		if dir_changed(dir):
			return True
	for file in files:
		if file_changed(file):
			return True
	return False

print "WATCHING FOR CHANGES!"
print "------------------------------------------------------------------"

done = False;
while True:
	changed = False
	while not changed:
		if process and process.poll() is not None and not done:
			done = True
			print "------------------------------------------------------------------"
			print "DONE, WATCHING FOR CHANGES!"
			print "------------------------------------------------------------------"
		time.sleep(1)
		if any_changed():
			if process and not done:
				stop_process()
			print "RESTARTING COMMAND: " + sys.argv[2]
			print "------------------------------------------------------------------"
			process = subprocess.Popen(sys.argv[2], shell=True, stdout=sys.stdout, stderr=sys.stderr)
			done = False
