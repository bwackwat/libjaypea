#!/usr/bin/python

# pip install requests

import requests, json, urllib3, getpass
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

base_url = "https://localhost:10443/api"

def TEST(method, route, data={}):
	response = None
	if(method == "GET"):
		response = requests.get(base_url + route, verify=False)
	elif(method == "POST"):
		response = requests.post(base_url + route, json=data, verify=False)
	elif(method == "PUT"):
		response = requests.put(base_url + route, json=data, verify=False)
	elif(method == "DELETE"):
		response = requests.delete(base_url + route, json=data, verify=False)
	else:
		print "Unknown Method: " + method
		return {}
	
	print response.request.method + " " + response.url
	print response.status_code
	if response.status_code != 200:
		print "Problem!"
		print response.headers
		print response.text
		exit(1)
	response_json = response.json()
	if len(response.content) < 200:
		print response_json
	if "error" in response_json:
		print data
		exit()
	print ""
	return response_json


TEST("GET", "/")

token = TEST("POST", "/login", {"username": raw_input("Username: "), "password": getpass.getpass("Password: ")})["token"]
TEST("POST", "/token", {"token": token})
TEST("POST", "/get/users", {"token": token})
TEST("POST", "/my/threads", {"token": token})

thread_id = TEST("POST", "/thread", {"token": token, "values": {"name": "Thread Name", "description": "Thread Desc"}})["id"]
TEST("GET", "/thread?id=" + thread_id)
TEST("PUT", "/thread", {"token": token, "id": thread_id, "values": {"name": "New Thread Name", "description": "New Thread Desc"}})

message_id = TEST("POST","/message",{"token":token,"values":{"thread_id":thread_id,"title":"Test Title","content":"Test Content"}})["id"]
TEST("GET", "/message?id=" + message_id)
TEST("PUT", "/message", {"token": token, "id": message_id, "values": {"title": "New Title", "content": "New Content"}})

TEST("GET", "/thread/messages/by/name?name=" + "New+Thread+Name")

TEST("DELETE", "/message", {"token": token, "id": message_id})
TEST("DELETE", "/thread", {"token": token, "id": thread_id})


