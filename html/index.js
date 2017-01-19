window.onload = function() {

var apiUrl = "https://" + window.location.hostname + "/api";

var status = document.getElementById("status");

function callAPI(method, route, data, callback){
	var sendData = JSON.stringify(data);

	var http = new XMLHttpRequest();
	http.open("POST", apiUrl + route, true);
	http.setRequestHeader("Content-type", "application/json");
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

////////////////////////////////////////////////////////////////
// BLOG
////////////////////////////////////////////////////////////////

var content = document.getElementById("content");
if(content !== null && content !== "undefined"){
	callAPI("/blog", {"username": "bwackwat"}, function(response){
		if(typeof(response.error) === 'undefined'){
			var newhtml = "<div id='posts'>";
			for(var i = 0, len = response.result.length; i < len; i++){
				newhtml += "<div id='post'><div id='posttitle'>" + response.result[i].title;
				newhtml += "</div><div id='postdate'>" + response.result[i].created_on;
				newhtml += "</div><br><div id='posttext'>" + response.result[i].content;
				newhtml += "</div></div><hr>";
			}
			newhtml += "</div>";
			content.innerHTML = newhtml;
		}else{
			content.innerHTML = response.error;
		}
	});
}

////////////////////////////////////////////////////////////////
// CMS
////////////////////////////////////////////////////////////////

var username = document.getElementById("usernameField");
var password = document.getElementById("passwordField");
var email = document.getElementById("emailField");
var firstName = document.getElementById("firstNameField");
var lastName = document.getElementById("lastNameField");

var blogTitle = document.getElementById("blogTitleField");
var blogContent = document.getElementById("blogContentField");
var posts = document.getElementById("posts");
var postEdit = document.getElementById("postEdit");

var loginButton = document.getElementById("loginButton");
var registerButton = document.getElementById("registerButton");
var submitPostButton = document.getElementById("submitPostButton");
var savePostButton = document.getElementById("savePostButton");
var logoutButton = document.getElementById("logoutButton");

function checkLogin(){
	if(localStorage.getItem(localStorageLoginTokenKey) !== null){
		callAPI("/token", {"token": localStorage.getItem(localStorageLoginTokenKey)}, function(response){
			if(typeof(response.error) === 'undefined'){
				status.innerHTML = "Logged in as " + localStorage.getItem(localStorageLoginUsernameKey);
				logoutButton.style.display = "inline";
			}else{
				localStorage.removeItem(localStorageLoginTokenKey);
				status.innerHTML = response.error;
			}
		});
	}else{
		status.innerHTML = "Not logged in.";
	}
	logoutButton.style.display = "none";
}

if(loginButton !== null && loginButton !== "undefined"){
	loginButton.onclick = function() {
		callAPI("/login", {"username": username.value, "password": password.value}, function(response){
			if(typeof(response.error) === 'undefined'){
				localStorage.setItem(localStorageLoginUsernameKey, username.value);
				localStorage.setItem(localStorageLoginTokenKey, response.result.token);
				location.reload();
			}else{
				status.innerHTML = response.error;
			}
		});
	};
}

if(registerButton !== null && registerButton !== "undefined"){
	registerButton.onclick = function() {
		callAPI("/user/new", {"username": username.value, "password": password.value, "email": email.value, "first_name": firstName.value, "last_name": lastName.value}, function(response){
			if(typeof(response.error) === 'undefined'){
				username.value = "";
				password.value = "";
				email.value = "";
				firstName.value = "";
				lastName.value = "";
				status.innerHTML = "You signed up successfully!";
			}else{
				status.innerHTML = response.error;
			}
		});
	};
}

if(logoutButton !== null && logoutButton !== "undefined"){
	logoutButton.onclick = function() {
		localStorage.removeItem(localStorageLoginTokenKey);
		location.reload();
	};
}

var blogId;
var blog = null;
selectPost = function(index){
	blogId = blog[index].id;
	blogTitle.value = blog[index].title;
	blogContent.value = blog[index].content;
	posts.style.display = "none";
	postEdit.style.display = "inline";
}

if(posts !== null && posts !== "undefined"){
	postEdit.style.display = "none";
	callAPI("/user/blog", {"token": localStorage.getItem(localStorageLoginTokenKey)}, function(response){
		if(typeof(response.error) === 'undefined'){
			blog = response.result;
			var newhtml = "";
			for(var i = 0, len = response.result.length; i < len; i++){
				newhtml += "<a href='#' onclick='selectPost(" + i + ");'>" + response.result[i].title + "</a><br>";
			}
			posts.innerHTML = newhtml;
		}else{
			status.innerHTML = response.error;
		}
	});
}

if(submitPostButton !== null && submitPostButton !== "undefined"){
	submitPostButton.onclick = function() {
		callAPI("/blog/new", {"token": localStorage.getItem(localStorageLoginTokenKey), "title": blogTitle.value, "content": blogContent.value}, function(response){
			if(typeof(response.error) === 'undefined'){
				blogTitle.value = "";
				blogContent.value = "";
				status.innerHTML = "Blog post successfully submitted!";
			}else{
				status.innerHTML = response.error;
			}
		});
	};
}

if(savePostButton !== null && savePostButton !== "undefined"){
	savePostButton.onclick = function() {
		callAPI("/blog/put", {"token": localStorage.getItem(localStorageLoginTokenKey), "id": blogId, "title": blogTitle.value, "content": blogContent.value}, function(response){
			if(typeof(response.error) === 'undefined'){
				status.innerHTML = "Blog post successfully saved!";
			}else{
				status.innerHTML = response.error;
			}
		});
	};
}

if(status !== null && status !== "undefined"){
	checkLogin();
}

}


