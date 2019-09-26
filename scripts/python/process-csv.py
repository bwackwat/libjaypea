#!/usr/bin/python

# https://www.sketchengine.co.uk/word-lists/

import re

with open("adjectives.js", "rw+") as file:
	nouns = file.readlines();
	for noun in nouns:
		if noun[0] == ';':
			continue
		word = re.search('[0-9]{1};([a-z]*);', noun)
		if word == None:
			continue
		file.write("\"" + word.group(1).title() + "\",\n")
	file.close()
