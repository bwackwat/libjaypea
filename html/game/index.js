window.onload = function() {

var status = document.getElementById("status");

var msg;

function connectionSuccess(e){
	msg = {};
	msg.username = prompt("Please enter your old or new username");
	msg.secret = prompt("Please enter your old or new secret");
	ws.send(JSON.stringify(msg));
}

function receivedPacket(e){
	console.log("RECV: " + e.data);
	console.log(e);
}

function connectionClosed(e){
	console.log("Closed.");
	console.log(e);
}

function connectionError(e){
	console.log("Error!");
	console.log(e);
}

var ws = new WebSocket("ws://" + window.location.hostname + ":11000/");
ws.onopen = connectionSuccess;
ws.onmessage = receivedPacket;
ws.onclose = connectionClosed;
ws.onerror = connectionError;

}
