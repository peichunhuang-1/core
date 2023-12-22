const queue_size = 100;
const data = {
    qx: new Array(queue_size),
    qy: new Array(queue_size),
    qz: new Array(queue_size),
    qw: new Array(queue_size),
    ax: new Array(queue_size),
    ay: new Array(queue_size),
    az: new Array(queue_size),
    tx: new Array(queue_size),
    ty: new Array(queue_size),
    tz: new Array(queue_size),
    timestamp: new Array(queue_size)
};
const data_accel = [{
    x: data.timestamp,
    y: data.ax,
    type: 'scatter',
    name: 'X-accel.'
}, {
    x: data.timestamp,
    y: data.ay,
    type: 'scatter',
    name: 'Y-accel.'
}, {
    x: data.timestamp,
    y: data.az,
    type: 'scatter',
    name: 'Z-accel.'
}];
const data_twist = [{
    x: data.timestamp,
    y: data.tx,
    type: 'scatter',
    name: 'X-twist'
}, {
    x: data.timestamp,
    y: data.ty,
    type: 'scatter',
    name: 'Y-twist'
}, {
    x: data.timestamp,
    y: data.tz,
    type: 'scatter',
    name: 'Z-twist'
}];
Plotly.newPlot('acceleration-chart-container', data_accel, {
    title: "3-Axis Acceleration",
    xaxis: {
    title: 'time (ms)'
    },
    yaxis: {
    title: 'accel. (g)'
    }
});
Plotly.newPlot('twist-chart-container', data_twist, {
    title: "3-Axis Twist",
    xaxis: {
    title: 'time (ms)'
    },
    yaxis: {
    title: 'twist. (rad/s)'
    }
});
let index = 0;
let ip_addr = window.prompt('srv addr?');
if (ip_addr) {
    const event_socket = new WebSocket('ws://'+ip_addr);
    event_socket.onmessage = function (event) {
        const message = JSON.parse(event.data);
        data.qx.push(message.qx);
        data.qy.push(message.qy);
        data.qz.push(message.qz);
        data.qw.push(message.qw);
        data.ax.push(message.ax);
        data.ay.push(message.ay);
        data.az.push(message.az);
        data.tx.push(message.tx);
        data.ty.push(message.ty);
        data.tz.push(message.tz);
        data.timestamp.push(message.timestamp);
        data.qx.shift();
        data.qy.shift();
        data.qz.shift();
        data.qw.shift();
        data.ax.shift();
        data.ay.shift();
        data.az.shift();
        data.tx.shift();
        data.ty.shift();
        data.tz.shift();
        data.timestamp.shift();
    };

    const plot_id = setInterval(() => {
        Plotly.update('acceleration-chart-container', {
            x: [data.timestamp, data.timestamp, data.timestamp],
            y: [data.ax, data.ay, data.az]});
        Plotly.update('twist-chart-container', {
            x: [data.timestamp, data.timestamp, data.timestamp],
            y: [data.tx, data.ty, data.tz]});
    }, 100);

    const calibration_cb = function() {
        const input = document.getElementById('time').value;
        event_socket.send(JSON.stringify({
            inteval: input
        }));
    }
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 100);

    const renderer = new THREE.WebGLRenderer();
    renderer.setClearColor(0xcccccc, 1.0);

    const canvasWrapper = document.getElementById('canvas-wrapper');
    const wrapperWidth = canvasWrapper.offsetWidth;
    const wrapperHeight = canvasWrapper.offsetHeight;
    camera.aspect = wrapperWidth / wrapperHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(wrapperWidth, wrapperHeight);

    document.getElementById('canvas-container').appendChild(renderer.domElement);

    const geometry = new THREE.BoxGeometry(1, 0.5, 0.1);
    const material = new THREE.MeshBasicMaterial({ color: 0xa0a0a0});
    cube = new THREE.Mesh(geometry, material);
    const edges = new THREE.EdgesGeometry( geometry ); 
    const line = new THREE.LineSegments(edges, new THREE.LineBasicMaterial( { color: 0x111111} ) ); 
    cube_with_edge = new THREE.Group();
    cube_with_edge.add(cube);
    cube_with_edge.add(line)
    scene.add(cube_with_edge);

    camera.position.set(1, 1, 1);
    camera.lookAt(0.5, 0.5, 0.5);

    const animate = function () {
        const quaternion = new THREE.Quaternion(data.qx.slice(-1)[0], data.qy.slice(-1)[0], data.qz.slice(-1)[0], data.qw.slice(-1)[0]);
        requestAnimationFrame(animate);
        cube_with_edge.setRotationFromQuaternion(quaternion);
        renderer.render(scene, camera);
    };
    animate();
}