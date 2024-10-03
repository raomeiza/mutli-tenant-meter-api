import express, { Request, Response } from 'express';
import http from 'http';
import socketIo from 'socket.io';
import cors from 'cors';

const app = express();
const server = http.createServer(app);

app.use(express.json());

const io = new socketIo.Server(server, {
  cors: {
    origin: "*",
    methods: "*"
  }
});

app.use(cors({
  origin: "*",
  methods: "*"
}));

interface Tenant {
  email: string;
  tenant_id: string;
  tenant_name: string;
  password: string;
}

const tenant_1: Tenant = {
  email: "abdulhameed.m1902991@st.futminna.edu.ng",
  tenant_id: "tenant_01",
  tenant_name: "Abdulraheem  A Omeiza",
  password: "qazxswedc",
};

const tenant_2: Tenant = {
  email: "omeiza.m1902991@st.futminna.edu.ng",
  tenant_id: "tenant_02",
  tenant_name: "R A Omeiza",
  password: "poiuytrewq",
};

const admin: Tenant = {
  email: "admin.m1902991@st.futminna.edu.ng",
  tenant_id: "admin_01",
  tenant_name: "R A Omeiza",
  password: "multitenantmeteradmin",
};

let tenant_1_power_comand = false;
let tenant_2_power_comand = false;
let rechargeTenant1 = 0;
let rechargeTenant2 = 0;

const userSockets: Map<string, string> = new Map();

io.on('connection', (socket) => {
  console.log('A user connected:', socket.id);

  socket.on('disconnect', () => {
    console.log('A user disconnected:', socket.id);
  });

  socket.on('identify', (id: string) => {
    socket.join(id);
    userSockets.set(id, socket.id);
    console.log(`User ${socket.id} identified as ${id}`);
    io.to('tenant_1').emit('message', 'Hello, tenant_1!');
  });

  io.to('tenant_1').emit('message', 'Hello, tenant_1!');
});

app.get('/', (req: Request, res: Response) => {
  res.send({ message: 'Hello! \n Welcome to the multitenant metering system api server.' });
});

interface LoginRequestBody {
  email: string;
  password: string;
}

app.post('/user/login', (req: any, res: any) => {
  const { email, password } = req.body;
  console.log(req.body);

  if (email === tenant_1.email && password === tenant_1.password) {
    return res.send({ tenant: { ...tenant_1, password: null } });
  } else if (email === tenant_2.email && password === tenant_2.password) {
    return res.send({ tenant: { ...tenant_2, password: null } });
  } else if (email === admin.email && password === admin.password) {
    return res.send({ tenant: { ...admin, password: null } });
  } else {
    return res.status(401).send({ error: 'Invalid email or password' });
  }
});

function sendMessageToUser(userId: string, message: any) {
  io.to(userId).emit('data', message);
}

app.get('/timestamp', (req: Request, res: Response) => {
  res.send({ timestamp: Date.now() });
});

interface PowerRequestBody {
  tenant_id: string;
}

app.post('/power', (req: Request<{}, {}, PowerRequestBody>, res: Response) => {
  if (req.body.tenant_id === 'tenant_01') {
    tenant_1_power_comand = true;
  } else if (req.body.tenant_id === 'tenant_02') {
    tenant_2_power_comand = true;
  }
  res.send({ success: true });
});

interface RechargeRequestBody {
  tenantId: string;
  amount: number;
}

app.post('/admin/recharge', (req: any, res: any) => {
  const { tenantId, amount } = req.body;
  if (tenantId === 'tenant_01') {
    rechargeTenant1 = amount;
  } else if (tenantId === 'tenant_02') {
    rechargeTenant2 = amount;
  } else {
    return res.status(401).send({ error: 'Invalid tenant_id' });
  }
  res.send({ success: true });
});

interface TenantStatus {
  tenant_id: string;
  status: boolean;
}

interface MainRequestBody {
  tenants: TenantStatus[];
}

app.post('/', (req: Request<{}, {}, MainRequestBody>, res: Response) => {
  req.body.tenants.forEach(tenant => {
    sendMessageToUser(tenant.tenant_id, tenant);
  });

  res.send({
    tenant_1_on: tenant_1_power_comand ? !req.body.tenants[0].status : req.body.tenants[0].status,
    tenant_2_on: tenant_2_power_comand ? !req.body.tenants[1].status : req.body.tenants[1].status,
    tenant_1_add_kwh: rechargeTenant1,
    tenant_2_add_kwh: rechargeTenant2,
  });

  tenant_1_power_comand = false;
  tenant_2_power_comand = false;
  rechargeTenant1 = 0;
  rechargeTenant2 = 0;
});

const PORT = process.env.PORT || 5000;
server.listen(PORT, () => {
  console.log(`Server is running on port ${PORT}`);
});

export default server;