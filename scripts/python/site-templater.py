#!/bin/python

import os, sys

if len(sys.argv) <= 1:
	print "First argument must be relative directory with templates."
	sys.exit(1)

os.stat(sys.argv[1])

def enter_directory(dir, current_template):
	items = os.listdir(dir)
	next_template = ""
	template_title = "MISSING TITLE" 	
	for item in items:
		citem = os.path.join(dir, item)
		if item == "template.html":
			if next_template != "":
				print "ERROR FOUND EXTRA TEMPLATE IN " + dir
				sys.exit(1)
			with open(citem, "r") as file:
				next_template = file.read()
		elif item == "nestedtemplate.html":
			if next_template != "":
				print "ERROR FOUND EXTRA TEMPLATE IN " + dir
				sys.exit(1)
			with open(citem, "r") as file:
				nested_content = file.read()
				title_start = nested_content.find("{TITLE:");
				title_end = nested_content.find("}", title_start);
				title = nested_content[title_start + 7:title_end]
				nested_content = nested_content.replace("{TITLE:" + title + "}", "")
				
				title_start = current_template.find("<title>")
				title_end = current_template.find("</title>", title_start)
				current_template = current_template[:title_start + 7] + title + current_template[title_end:]
				
				next_template = current_template.replace("{CONTENT}", nested_content)
	for item in items:
		citem = os.path.join(dir, item)
		if citem.endswith(".template.html"):
			new_item = citem.replace(".template.html", ".html")
			print "Replacing {CONTENT} in " + citem + " to " + new_item
			with open(citem, "r") as oldfile, open(new_item, "w+") as newfile:
				new_content = oldfile.read()
				
				title_start = new_content.find("{TITLE:");
				if title_start > -1:
					title_end = new_content.find("}", title_start);
					title = new_content[title_start + 7:title_end]
					new_content = new_content.replace("{TITLE:" + title + "}", "")
					next_template = next_template.replace("{TITLE}", title)
					
					print "FOUND TITLE " + title + " IN " + citem
					
					title_start = next_template.find("<title>")
					title_end = next_template.find("</title>", title_start)
					next_template = next_template[:title_start + 7] + title + next_template[title_end:]
				
				newfile.write(next_template.replace("{CONTENT}", new_content))
	for item in items:
		citem = os.path.join(dir, item)
		if os.path.isdir(citem):
			enter_directory(citem, next_template)

enter_directory(sys.argv[1], "{CONTENT}")

print "DONE!"
