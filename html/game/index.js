window.onload = function() {

var status = document.getElementById("status");

function connectionSuccess(e){
	console.log("Success!");
	console.log(e);
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
