#!/bin/python

import os, sys

if len(sys.argv) <= 1:
	print "First argument must be relative directory with templates."
	sys.exit(1)

os.stat(sys.argv[1])

def enter_directory(dir, current_template):
	items = os.listdir(dir)
	next_template = ""
	for item in items:
		citem = os.path.join(dir, item)
		if item == "template.html":
			if next_template != "":
				print "ERROR FOUND EXTRA TEMPLATE IN " + dir
				sys.exit(1)
			with open(citem, "r") as file:
				next_template = file.read()
		if item == "nestedtemplate.html":
			if next_template != "":
				print "ERROR FOUND EXTRA TEMPLATE IN " + dir
				sys.exit(1)
			with open(citem, "r") as file:
				next_template = current_template.replace("{CONTENT}", file.read())
	for item in items:
		citem = os.path.join(dir, item)
		if citem.endswith(".template.html"):
			new_item = citem.replace(".template.html", ".html")
			print "Replacing {CONTENT} in " + citem + " to " + new_item
			with open(citem, "r") as oldfile, open(new_item, "w+") as newfile:
				newfile.write(next_template.replace("{CONTENT}", oldfile.read()))
	for item in items:
		citem = os.path.join(dir, item)
		if os.path.isdir(citem):
			enter_directory(citem, next_template)

enter_directory(sys.argv[1], "{CONTENT}")

print "DONE!"
