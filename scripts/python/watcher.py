import subprocess, os, sys, time, atexit, signal, psutil

if len(sys.argv) < 2:
	print "Usage: watcher.py <directory to watch> <command to terminate and repeat>"
	sys.exit(1)

print "------------------------------------------------------------------"

print "DIRECTORY: " + sys.argv[1]
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

files = {}

while True:
	checking_for_changes = True
	while checking_for_changes:
		time.sleep(1)
		for file in os.listdir(sys.argv[1]):
			mtime = os.stat(sys.argv[1] + file).st_mtime
			if not file in files:
				files[file] = mtime
			else:
				if files[file] != mtime:
					files[file] = mtime
					checking_for_changes = False
					stop_process()
					process = start_process()
					print "------------------------------------------------------------------"
					print "CHANGE: " + file
					print "RESTARTING"
					print "------------------------------------------------------------------"
					break
