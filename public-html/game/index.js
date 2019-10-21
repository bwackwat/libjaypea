window.onload = function() {

var msg;

function connectionSuccess(e){
	msg = {};
	//msg.username = prompt("Please enter your old or new username");
	//msg.secret = prompt("Please enter your old or new secret");
	msg.username = 'test';
	msg.password = 'pass';
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

window.onkeyup = function(e){
	msg = {};
	msg.type = "key";
	msg.key = e.code;
	str = JSON.stringify(msg);
	console.log("SEND: " + str);
	ws.send(str);
}

}
