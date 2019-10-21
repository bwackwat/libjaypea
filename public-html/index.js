window.onload = function() {

var apiUrl = window.location.protocol + "//" + window.location.hostname + ":" + window.location.port + "/api";

var messages = document.getElementById("messages");
var submitButton = document.getElementById("submitButton");
var submitField = document.getElementById("submitField");

function callAPI(method, route, data, callback){
	var sendData = JSON.stringify(data);
	var http = new XMLHttpRequest();
	http.open(method, apiUrl + route, true);
	http.setRequestHeader("Content-Type", "application/json");
	http.onreadystatechange = function(){
		if(http.responseText === ""){
			//Bloody OPTIONS pre-flight...
			return;
		}
		console.log("RECV: " + http.responseText);
		if(http.readyState == 4){
			if(http.status == 200){
				callback(JSON.parse(http.responseText));
			}else{
				callback({"error":"Bad response from server."})
			}
		}else if(http.readyState == 3){
			//Bogus OPTIONS response...
			
			//0: request not initialized
			//1: server connection established
			//2: request received
			//3: processing request
			//4: request finished and response is ready
		}else if(http.readyState == 2){
			callback({"error":"Could not receive data."})
		}else if(http.readyState == 1){
			callback({"error":"Could not establish connection."})
		}else if(http.readyState == 0){
			callback({"error":"Did not start connection."})
		}else{
			//Invalid API usage...
			alert("HTTP ERROR!");
		}
	};
	http.send(sendData);
}

function getMessages(){
	callAPI("GET", "/message", {}, function(response){
		if(typeof(response.error) === 'undefined'){
			var newhtml = "";
			for(var i = 0, len = response.result.length; i < len; i++){
				newhtml += "<div>" + response.result[i] + "</div>";
			}
			messages.innerHTML = newhtml;
		}else{
			messages.innerHTML = "<h2>" + response.error + "</h2>";
		}
	});
}

if(submitButton !== null && submitButton !== "undefined"){
	submitButton.onclick = function(){
		callAPI("POST", "/message", {"message": submitField.value}, function(response){
			if(typeof(response.error) === 'undefined'){
				getMessages();
			}else{
				messages.innerHTML = "<h2>" + response.error + "</h2>";
			}
		});
	};
}

getMessages();

}
