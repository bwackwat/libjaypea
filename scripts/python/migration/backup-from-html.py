#!/usr/bin/python3

import sys, dateutil.parser, psycopg2
from bs4 import BeautifulSoup

#pip install saws beautifulsoup4 psycopg2
	
if len(sys.argv) < 2:
	print("Usage: backup-from-html.py <html file>")
	sys.exit(1)

try:
	conn = psycopg2.connect("dbname='webservice' user='postgres' password='abc123'")
	cur = conn.cursor()
except:
	print("bad db connection")
	sys.exit(1)

reload(sys)
sys.setdefaultencoding('utf8')

html_file = sys.argv[1]

reload(sys)
sys.setdefaultencoding('utf8')

soup = BeautifulSoup(open(html_file, "r").read())

for post in soup.findAll("div", {"id": "post"}):

	# GET OWNER ID
	# GET THREAD ID
	# SELECT id FROM users WHERE username = 'bwackwat';
	# SELECT id FROM threads WHERE name = 'Grokking Equanimity';

	# USE PSYCOPG2 FOR STRING ESCAPING
	
	cur.execute("INSERT INTO messages (owner_id, thread_id, title, content, created, modified) VALUES (%s, %s, %s, %s, %s, now());", (1, 1, post.find(id="posttitle").decode_contents(formatter="html"), post.find(id="posttext").decode_contents(formatter="html"), post.find(id="postdate").decode_contents(formatter="html")))

conn.commit()
cur.close()


