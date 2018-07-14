#!/bin/python

import sys, os, datetime, shutil

print "Arguments: %s" % (sys.argv)

if len(sys.argv) <= 1:
	print "First argument must be the directory to create index HTML for."
	sys.exit(1)

root_dir = sys.argv[1]
print "Indexing directory " + root_dir

index_html_template = "<html>" \
			"<head>" \
			"<title>Index of %s</title>" \
			"</head>" \
			"<body>" \
			"<h1>Index of %s</h1>" \
			"<h4>" + str(datetime.datetime.now()) + "</h4>" \
			"<hr>" \
			"%s" \
			"</body>" \
			"</html>"

def rmcheck():
	if(raw_input("rm -rf " + root_dir + "index/ ? [y]: ") or "y" != "y"):
		print "Exiting"
		sys.exit(1)

if(len(sys.argv) > 2):
	if(sys.argv[2] != "y"):
		rmcheck()
else:
	rmcheck()

if os.path.exists(root_dir + "index/"):
	shutil.rmtree(root_dir + "index/")

def enter_directory(next_dir):
	items = os.listdir(next_dir)
	links = []

	if next_dir == root_dir:
		base_dir = "/"
		links.append("<a href=\"/\">../</a>")
		html_file_name = os.path.join(root_dir, "index/index.html")
	else:
		base_dir = "/" + os.path.relpath(next_dir, root_dir)
		base_dir = base_dir.replace("\\", "/")
		links.append("<a href=\"/index%s\">../</a>" % (os.path.split(base_dir)[:-1]))
		index_dir = os.path.join(root_dir, "index")
		html_file_name = index_dir + ("%s/index.html" % (base_dir))
	html_file_name = html_file_name.replace("\\", "/")

	for item in items:
		item_path = os.path.join(next_dir, item)
		base_item_path = os.path.join(base_dir, item)
		base_item_path = base_item_path.replace("\\", "/")
		if os.path.isfile(item_path):
			links.append("<a href=\"%s\">%s</a>" % (base_item_path, item))
		elif os.path.isdir(item_path):
			links.append("<a href=\"/index%s/index.html\">%s/</a>" % (base_item_path, item))
		else:
			print "Unknown item: %s" % (item_path)

	if not os.path.exists(os.path.dirname(html_file_name)):
		os.makedirs(os.path.dirname(html_file_name))

	with open(html_file_name, "w+") as html_file:
		html_file.write(index_html_template % (base_dir, base_dir, "<br>".join(links)))
	print "Wrote: %s" % html_file_name

	for item in items:
		print item
		item_path = os.path.join(next_dir, item)
		if os.path.isdir(item_path) and item != "index":
			enter_directory(item_path)

enter_directory(root_dir)

print "DONE!"
