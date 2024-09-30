const express = require('express');
const socket = require('socket.io');
const server = require('../bin/www');
const cors = require('cors');
// initialize socket
const io = socket(server, {
  cors: {
    origin: '*',
    methods: '*'
  }
});
const userSockets = new Set();
io.on('connection', (socket) => {
  console.log('A user connected:', socket.id);

  // Handle the identify event
  socket.on('identify', (id) => {
    socket.join(id);
    userSockets[id] = socket.id;
    console.log(`User ${socket.id} identified as ${id}`);
  });

  // Example of sending a message to a specific socket ID
  io.to('tenant_1').emit('message', 'Hello, tenant_1!');
});
const router = express.Router();

router.get('/', (req, res) => {
  console.log(req.body);
  res.send({ message: 'Hello world' });
});

router.post('/user/login', (req, res) => {
  console.log(req.body);
  const { username, password } = req.body;
  if (username === 'admin' && password === 'admin') {
    userSockets.add(req.body.userId);
    res.send({ message: 'Login success' });
  } else {
    res.send({ message: 'Login failed' });
  }
});

function sendMessageToUser(userId, message) {
  const socketId = userSockets[userId];
  if (socketId) {
    io.to(socketId).emit('message', message);
  }
}

router.post('/', (req, res) => {
  console.log(req.body.tenants);
  req.body.tenants.forEach(tenant => {
    sendMessageToUser(tenant.tenant_id, tenant);
  });
  res.send({
    // tenant_1_clear: true,
    // tenant_2_clear: true,
    // tenant_1_add_kwh: Math.random()*100,
    // tenant_2_add_kwh: Math.random()*100,
    // tenant_1_on: true,
    // tenant_2_on: true
  });
});

module.exports = router;
