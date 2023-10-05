const protobuf = require('protobufjs');
const ServiceClient = require('./../core/ServiceClient.js');
const Subscriber = require('./../core/Subscriber.js');
const http = require('http');
const fs = require('fs');
const WebSocket = require('ws');
const path = require('path');  

const root = protobuf.loadSync(`${process.env.HOME}/Desktop/core/example/protos`+"/imu.proto");
function reply_call_back(reply) {
    console.log(reply);
}
let qx = 0, qy = 0, qz = 0, qw = 1;
let ax = 0, ay = 0, az = 1;
let tx = 0, ty = 0, tz = 0;
let timestamp = 0;
function subscriber_callback(request) {
    ax = request.acceleration.x; ay = request.acceleration.y; az = request.acceleration.z;
    tx = request.twist.x; ty = request.twist.y;tz = request.twist.z;
    qx = request.attitude.x; qy = request.attitude.y; qz = request.attitude.z; qw = request.attitude.w; 
    timestamp = request.timestamp;
}
const imu_proto = root.lookup('sensor_msg.imu');
const imu_sub = Subscriber("imu", 1000, subscriber_callback, imu_proto);

const imu_clt = ServiceClient("imu", reply_call_back, root, 'sensor_msg.imu_calibration_request', 'sensor_msg.imu_calibration_reply');

const server = http.createServer(function(request, response) {
fs.readFile(path.join(__dirname, 'index', 'index.html'), function(error, data) {
    if (error) {
    response.writeHead(500);
    response.end('Error loading index.html');
    console.log(__dirname);
    } else {
    response.writeHead(200, { 'Content-Type': 'text/html' });
    response.end(data);
    }
});
});

const port = 8080;
server.listen(port, '0.0.0.0', function() {
  console.log(`Server is running on port ${port}`);
});

const wss = new WebSocket.Server({ server });
wss.on('connection', function(ws) {
console.log('Client connected');
intervalId = setInterval(() => {
    ws.send(JSON.stringify({
        qx : qx,
        qy : qy,
        qz : qz,
        qw : qw,
        ax: ax,
        ay: ay,
        az: az,
        tx: tx,
        ty: ty, 
        tz: tz,
        timestamp: timestamp
    }));
}, 100);
ws.on('message', function(message) {
    const data = JSON.parse(message);
    imu_clt.send({
        inteval: data.inteval
    });
});

ws.on('close', function() {
    console.log('Client disconnected');
});
});