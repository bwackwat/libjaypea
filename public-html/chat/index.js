window.onload = function(){

var status = document.getElementById("status");

var handleField = document.getElementById("handleField");
var messageField = document.getElementById("messageField");
var submitButton = document.getElementById("submitButton");
var messages = document.getElementById("messages");

var ws = new WebSocket("ws://" + window.location.hostname + ":11000/");
var msg = {};

function ping(){
	console.log("ping " + ws.readyState);
	if(ws.readyState === ws.CLOSED){
		ws = new WebSocket("ws://" + window.location.hostname + ":11000/");
		status.innerHTML = "Connecting...";
	}else if(ws.readyState === ws.OPEN){
		status.innerHTML = "Connected.";
		ws.send("ping")
	}else{
		status.innerHTML = "Working...";
	}
}
var pinger = setInterval(ping, 1000);

ws.onopen = function(e){
	console.log(e);
	status.innerHTML = "Connected.";
}

ws.onmessage = function(e){
	if(e.data == "pong"){
		return;
	}
	console.log("RECV:" + e.data);
	msg = JSON.parse(e.data);
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

function send(e){
	status.innerHTML = "";
	clearInterval(pinger);
	pinger = setInterval(ping, 1000);
	msg = {};
	msg.handle = handleField.value;
	msg.message = messageField.value;
	messageField.value = "";
	messageField.focus();

	console.log("SEND:" + JSON.stringify(msg));
	ws.send(JSON.stringify(msg));
}

window.onkeypress = function(e){
	if(e.code === "Enter"){
		send(e);
	}else{
		console.log(e.code);
	}
}

submitButton.onclick = send;

//function(e){
//	status.innerHTML = "";
//	msg = {};
//	msg.handle = handleField.value;
//	msg.message = messageField.value;

//	console.log("SEND:" + JSON.stringify(msg));
//	ws.send(JSON.stringify(msg));
//}

}
