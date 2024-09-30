const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');
const app = express();
const server = http.createServer(app);
// accept json data
app.use(express.json());
const io = socketIo(server, {
  cors: {
    origin: "*", // Allow requests from this origin
    methods: "*"
  }
}); // Attach socket.io to the HTTP server

app.use(cors({
  origin: "*", // Allow requests from this origin
  methods: "*"
}));

var tenant_1_power_comand = false;
var tenant_2_power_comand = false;
const tenant_1 = {
  email: "abdulhameed.m1902991@st.futminna.edu.ng",
  tenant_id: "tenant_01",
  tenant_name: "Abdulraheem  A Omeiza",
  password: "qazxswedc",
}
const tenant_2 = {
  email: "omeiza.m1902991@st.futminna.edu.ng",
  tenant_id: "tenant_02",
  tenant_name: "R A Omeiza",
  password: "poiuytrewq",
}

const admin = {
  email: "admin.m1902991@st.futminna.edu.ng",
  tenant_id: "admin_01",
  tenant_name: "R A Omeiza",
  password: "multitenantmeteradmin",
}

var rechargeTenant1 = 0;
var rechargeTenant2 = 0;

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
    io.to('tenant_1').emit('message', 'Hello, tenant_1!');
  });

  // Example of sending a message to a specific socket ID
  io.to('tenant_1').emit('message', 'Hello, tenant_1!');
});


app.get('/', (req, res) => {
  res.send({ message: 'Hello world' });
});

app.post('/user/login', (req, res) => {
  const { email, password } = req.body;
  console.log(req.body);
  // of the 2 tenants data we have above, check if the email and password match any of them
  if (email === tenant_1.email && password === tenant_1.password) {
    return res.send({ tenant: {...tenant_1, password: null} });
  } else if (email === tenant_2.email && password === tenant_2.password) {
    return res.send({ tenant: {...tenant_2, password: null} });
  } else if (email === admin.email && password === admin.password) {
    return res.send({ tenant: {...admin, password: null} });
  }  else {
    return res.status(401).send({ error: 'Invalid email or password' });
  }
});

function sendMessageToUser(userId, message) {
  // console.log('Sending message to user:', userId, message);
  io.to(userId).emit('data', message);
  // const socketId = userSockets[userId];
  // if (socketId) {
  // }
}

app.get('/timestamp', (req, res) => {
  res.send({ timestamp: Date.now() });
});
app.post('/power', (req, res) => {
  // console.log('pppppppoweeeeeerr', req.body);
  if (req.body.tenant_id === 'tenant_01') {
    tenant_1_power_comand = true
  } else if (req.body.tenant_id === 'tenant_02') {
    tenant_2_power_comand = true
  }
  res.send({ success: true });
});


app.post('/admin/recharge', (req, res) => {
  const { tenantId, amount } = req.body;
  if(tenantId === 'tenant_01') {
    rechargeTenant1 = amount;
  } else if(tenantId === 'tenant_02') {
    rechargeTenant2 = amount;
  } else {
    return res.status(401).send({ error: 'Invalid tenant_id' });
  }
  res.send({ success: true });
});

app.post('/', (req, res) => {
  // console.log( req.body.tenants);
  // console.log('pppppppoweeeeeerr', tenant_1_power_comand, tenant_2_power_comand);
  req.body.tenants.forEach(tenant => {
    sendMessageToUser(tenant.tenant_id, tenant);
  });
  
  res.send({
    tenant_1_on: tenant_1_power_comand ? !req.body.tenants[0].status : req.body.tenants[0].status,
    tenant_2_on: tenant_2_power_comand ? !req.body.tenants[1].status : req.body.tenants[1].status,
    // tenant_1_clear: true,
    // tenant_2_clear: true,
    tenant_1_add_kwh: rechargeTenant1,
    tenant_2_add_kwh: rechargeTenant2,
  });
  tenant_1_power_comand = false
  tenant_2_power_comand = false
  rechargeTenant1 = 0;
  rechargeTenant2 = 0;
});

const PORT = process.env.PORT || 5000;
server.listen(PORT, () => {
  console.log(`Server is running on port ${PORT}`);
});

module.exports = server;