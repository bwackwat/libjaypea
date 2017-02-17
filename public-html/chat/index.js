window.onload = function(){

var status = document.getElementById("status");

var handleField = document.getElementById("handleField");
var messageField = document.getElementById("messageField");
var submitButton = document.getElementById("submitButton");
var messages = document.getElementById("messages");

var ws = new WebSocket("ws://" + window.location.hostname + ":11000/");
var msg = {};

ws.onopen = function(e){
	console.log(e);
	status.innerHTML = "Connected.";
}

ws.onmessage = function(e){
	console.log("RECV:" + JSON.stringify(e.data));
	if(typeof e.data.handle !== 'undefined' && typeof e.data.message !== 'undefined'){
		messages.innerHTML = "<p>" + e.data.handle + ": " + e.data.message + "</p>" + messages.innerHTML;
	}else if(typeof e.data.status !== 'undefined'){
		status.innerHTML = e.data.status;
	}
}

ws.onclose = function(e){
	console.log(e);
	status.innerHTML = "Disconnected.";
}

ws.onerror = function(e){
	console.log(e);
	status.innerHTML = "Error.";
}

submitButton.onclick = function(e){
	msg = {};
	msg.handle = handleField.value;
	msg.message = messageField.value;

	console.log("SEND:" + JSON.stringify(msg));
	ws.send(JSON.stringify(msg));
}

}
