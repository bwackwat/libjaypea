#!/usr/bin/python

# pip install requests

import requests, json, urllib3, getpass

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

hostname = "localhost"
http_port = "10080"
https_port = "10443"

def test(request):
	print request.request.method + " " + request.url
	if request.status_code != 200:
		print "Problem!"
		print request.headers
		print request.text
		exit(1)
	if len(request.content) > 200:
		print "200"
	else:
		print request.json()
	print ""
	return request

test(requests.get("https://" + hostname + ":" + https_port + "/", verify=False))
test(requests.get("https://" + hostname + ":" + https_port + "/api", verify=False))

token = test(requests.post("https://" + hostname + ":" + https_port + "/api/login", json={"username": raw_input("Username: "), "password": getpass.getpass("Password: ")}, verify=False)).json()["token"]

test(requests.post("https://" + hostname + ":" + https_port + "/api/token", json={"token": token}, verify=False))
test(requests.post("https://" + hostname + ":" + https_port + "/api/get/users", json={"token": token}, verify=False))

thread_id = test(requests.post("https://" + hostname + ":" + https_port + "/api/my/threads", json={"token": token}, verify=False)).json()[0]["id"]

test(requests.get("https://" + hostname + ":" + https_port + "/api/thread", json={"id": thread_id}, verify=False))

thread_id = test(requests.post("https://" + hostname + ":" + https_port + "/api/thread", json={"token": token, "values": ["Thread Name", "Thread Desc"]}, verify=False)).json()["id"]

test(requests.put("https://" + hostname + ":" + https_port + "/api/thread", json={"token": token, "id": thread_id, "values": {"name": "New Thread Name", "description": "New Thread Desc"}}, verify=False))

message_id = test(requests.post("https://" + hostname + ":" + https_port + "/api/message", json={"token": token, "values": [thread_id, "Test Title", "Test Content"]}, verify=False)).json()["id"]

test(requests.get("https://" + hostname + ":" + https_port + "/api/message", json={"id": message_id}, verify=False))

test(requests.get("https://" + hostname + ":" + https_port + "/api/thread/messages", json={"id": thread_id}, verify=False))

test(requests.put("https://" + hostname + ":" + https_port + "/api/message", json={"token": token, "id": message_id, "values": {"title": "New Title", "content": "New Content"}}, verify=False))

test(requests.get("https://" + hostname + ":" + https_port + "/api/thread/messages/by/name", json={"name": "New Thread Name"}, verify=False))

test(requests.delete("https://" + hostname + ":" + https_port + "/api/message", json={"token": token, "id": message_id}, verify=False))

test(requests.delete("https://" + hostname + ":" + https_port + "/api/thread", json={"token": token, "id": thread_id}, verify=False))


