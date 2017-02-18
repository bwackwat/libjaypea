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
	msg = JSON.parse(e.data);
	console.log("RECV:" + e.data);
	if(typeof msg.handle !== 'undefined' && typeof msg.message !== 'undefined'){
		messages.innerHTML = "<p>" + msg.handle + ": " + msg.message + "</p>" + messages.innerHTML;
	}else if(typeof msg.status !== 'undefined'){
		status.innerHTML = msg.status;
	}else{
		console.log("Received junk?");
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
	status.innerHTML = "";
	msg = {};
	msg.handle = handleField.value;
	msg.message = messageField.value;

	console.log("SEND:" + JSON.stringify(msg));
	ws.send(JSON.stringify(msg));
}

}
