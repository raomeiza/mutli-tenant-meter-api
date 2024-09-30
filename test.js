const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');
const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: {
    origin: "http://localhost:3000", // Allow requests from this origin
    methods: ["GET", "POST"]
  }
}); // Attach socket.io to the HTTP server

app.use(cors({
  origin: "http://localhost:3000", // Allow requests from this origin
  methods: ["GET", "POST"]
}));

const userSockets = new Set();

io.on('connection', (socket) => {
  console.log('A user connected:', socket.id);

  socket.on('disconnect', () => {
    console.log('A user disconnected:', socket.id);
  });

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
  res.send({ message: 'Hello world' });
});

router.post('/user/login', (req, res) => {
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
  req.body.tenants.forEach(tenant => {
    sendMessageToUser(tenant.tenant_id, tenant);
  });
  res.send({});
});

app.use('/', router);

const PORT = process.env.PORT || 3001;
server.listen(PORT, () => {
  console.log(`Server is running on port ${PORT}`);
});

module.exports = server;