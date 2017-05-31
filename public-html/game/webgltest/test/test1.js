"use strict";

var textCanvas = document.createElement("canvas").getContext("2d");

function setupTextCanvas(text, width, height) {
    textCanvas.canvas.width  = width;
    textCanvas.canvas.height = height;
    textCanvas.font = "20px monospace";
    textCanvas.textAlign = "center";
    textCanvas.textBaseline = "middle";
    textCanvas.strokeStyle = "blue";
    textCanvas.fillStyle = "red";
    textCanvas.clearRect(0, 0, textCanvas.canvas.width, textCanvas.canvas.height);
    textCanvas.fillText(text, width / 2, height / 2);
    textCanvas.strokeText(text, width / 2, height / 2);
    return textCanvas.canvas;
}

window.onload = function(){

var canvas = document.getElementById('my_Canvas');
var canvasDefaultWidth = canvas.width;
var canvasDefaultHeight = canvas.height;

var gl = canvas.getContext('webgl');
if(!gl){
    console.log("Your browser doesn't support WebGL!");
    return;
}

window.onresize = function(event) {
    if(window.innerWidth < canvasDefaultWidth){
        canvas.width = window.innerWidth;
    }else{
        canvas.width = canvasDefaultWidth;
    }
    if(window.innerHeight < canvasDefaultHeight){
        canvas.height = window.innerHeight;
    }else{
        canvas.height = canvasDefaultHeight;
    }
    gl.viewport(0, 0, canvas.width, canvas.height);
};

var PICK = 0;
var PLAY = 1;
var state = undefined;

var mouseX = 0;
var mouseY = 0;
canvas.onmousemove = function(e){
    mouseX = e.offsetX / canvas.width * 2.0 - 1;
    mouseY = e.offsetY / canvas.height * 2.0 - 1;

    // THESE ARE ABSOLUTE TO THE SCREEN, offsetX/offsetY are RELATIVE TO THE CANVAS
    //var x = e.clientX;
    //var y = e.clientY;

    //console.log(mouseX + ", " + mouseY);
}

var grid_width = 5;
var grid_height = 5;

var grid_vertices = [];
for(var x = -1.0, cell_width = 2.0 / grid_width; x <= 1.0; x += cell_width){
    grid_vertices.push(x);
    grid_vertices.push(-1.0);
    grid_vertices.push(0.0);

    grid_vertices.push(x);
    grid_vertices.push(1.0);
    grid_vertices.push(0.0);
}
for(var y = -1.0, cell_height = 2.0 / grid_height; y <= 1.0; y += cell_height){
    grid_vertices.push(-1.0);
    grid_vertices.push(y);
    grid_vertices.push(0.0);

    grid_vertices.push(1.0);
    grid_vertices.push(y);
    grid_vertices.push(0.0);
}

var vertex_buffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, vertex_buffer);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(grid_vertices), gl.STATIC_DRAW);
gl.bindBuffer(gl.ARRAY_BUFFER, null);

var left_arrow_vertices = [
    -0.8, 0, 0,
    -0.7, -0.1, 0,
    -0.7, 0.1, 0
];

var left_arrow_buffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, left_arrow_buffer);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(left_arrow_vertices), gl.STATIC_DRAW);
gl.bindBuffer(gl.ARRAY_BUFFER, null);

var right_arrow_vertices = [
    0.8, 0, 0,
    0.7, -0.1, 0,
    0.7, 0.1, 0
];

var right_arrow_buffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, right_arrow_buffer);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(right_arrow_vertices), gl.STATIC_DRAW);
gl.bindBuffer(gl.ARRAY_BUFFER, null);

var indices = [0, 1, 1, 2, 2, 0]; 

var arrow_index_buffer = gl.createBuffer();
 gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, arrow_index_buffer);
 gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(indices), gl.STATIC_DRAW);
 gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);

var continue_vertices = [
    -0.5, -0.4, 0,
    -0.5, -0.6, 0,
    0.5, -0.4, 0,
    0.5, -0.6, 0
];

var continue_buffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, continue_buffer);
gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(continue_vertices), gl.STATIC_DRAW);
gl.bindBuffer(gl.ARRAY_BUFFER, null);

var continue_indices = [0, 1, 2, 3, 1, 2];

var continue_index_buffer = gl.createBuffer();
 gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, continue_index_buffer);
 gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, new Uint16Array(continue_indices), gl.STATIC_DRAW);
 gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, null);

var textCanvas = setupTextCanvas("Continue", 140, 26);
var textWidth  = textCanvas.width;
var textHeight = textCanvas.height;
var textTexture = gl.createTexture();

gl.bindTexture(gl.TEXTURE_2D, textTexture);

gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);

gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, textCanvas);

gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
//gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_NEAREST);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

//gl.generateMipmap(gl.TEXTURE_2D);

gl.bindTexture(gl.TEXTURE_2D, null);

var vertCode =
'attribute vec3 coordinates;' +
'void main(void) {' +
   ' gl_Position = vec4(coordinates, 1.0);' +
'}';
var vertShader = gl.createShader(gl.VERTEX_SHADER);
gl.shaderSource(vertShader, vertCode);
gl.compileShader(vertShader);

var fragCode =
'void main(void) {' +
   'gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);' +
'}';
var fragShader = gl.createShader(gl.FRAGMENT_SHADER);
gl.shaderSource(fragShader, fragCode);
gl.compileShader(fragShader);

var shaderProgram = gl.createProgram();
gl.attachShader(shaderProgram, vertShader);
gl.attachShader(shaderProgram, fragShader);
gl.linkProgram(shaderProgram);
gl.useProgram(shaderProgram);

function checkState(){
    if(state == PICK){
        state = PLAY;
    }else{
        state = PICK;
    }
}

canvas.onmouseup = function(e){
    checkState();
}

checkState();

function render(){
    gl.clearColor(0.0, 0.0, 0.0, 1.0);
    gl.enable(gl.DEPTH_TEST);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

    if(state == PICK){
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, arrow_index_buffer);

        gl.bindBuffer(gl.ARRAY_BUFFER, left_arrow_buffer);

        var coord = gl.getAttribLocation(shaderProgram, "coordinates");
        gl.vertexAttribPointer(coord, 3, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(coord);

        if(mouseX < -0.7 && mouseX > -0.8 && mouseY < 0.1 && mouseY > -0.1){
            gl.drawArrays(gl.TRIANGLES, 0, left_arrow_vertices.length / 3);
        }else{
            gl.drawElements(gl.LINES, indices.length, gl.UNSIGNED_SHORT, 0);
        }

        gl.bindBuffer(gl.ARRAY_BUFFER, right_arrow_buffer);

        var coord = gl.getAttribLocation(shaderProgram, "coordinates");
        gl.vertexAttribPointer(coord, 3, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(coord);

        if(mouseX > 0.7 && mouseX < 0.8 && mouseY < 0.1 && mouseY > -0.1){
            gl.drawArrays(gl.TRIANGLES, 0, right_arrow_vertices.length / 3);
        }else{
            gl.drawElements(gl.LINES, indices.length, gl.UNSIGNED_SHORT, 0);
        }

        gl.bindBuffer(gl.ARRAY_BUFFER, continue_buffer);
        gl.activeTexture(gl.TEXTURE0);
        gl.bindTexture(gl.TEXTURE_2D, textTexture);
        gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, continue_index_buffer);

        var coord = gl.getAttribLocation(shaderProgram, "coordinates");
        gl.vertexAttribPointer(coord, 3, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(coord);

        gl.drawElements(gl.TRIANGLES, continue_indices.length, gl.UNSIGNED_SHORT, 0);
    }else{
        gl.bindBuffer(gl.ARRAY_BUFFER, vertex_buffer);
        var coord = gl.getAttribLocation(shaderProgram, "coordinates");
        gl.vertexAttribPointer(coord, 3, gl.FLOAT, false, 0, 0);
        gl.enableVertexAttribArray(coord);

        gl.drawArrays(gl.LINES, 0, grid_vertices.length / 3);
    }

    requestAnimationFrame(render);
}

render();

}