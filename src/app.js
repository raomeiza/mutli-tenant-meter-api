"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const express_1 = __importDefault(require("express"));
const http_1 = __importDefault(require("http"));
const socket_io_1 = __importDefault(require("socket.io"));
const cors_1 = __importDefault(require("cors"));
const app = (0, express_1.default)();
const server = http_1.default.createServer(app);
app.use(express_1.default.json());
const io = new socket_io_1.default.Server(server, {
    cors: {
        origin: "*",
        methods: "*"
    }
});
app.use((0, cors_1.default)({
    origin: "*",
    methods: "*"
}));
const tenant_1 = {
    email: "abdulhameed.m1902991@st.futminna.edu.ng",
    tenant_id: "tenant_01",
    tenant_name: "Abdulraheem  A Omeiza",
    password: "qazxswedc",
};
const tenant_2 = {
    email: "omeiza.m1902991@st.futminna.edu.ng",
    tenant_id: "tenant_02",
    tenant_name: "R A Omeiza",
    password: "poiuytrewq",
};
const admin = {
    email: "admin.m1902991@st.futminna.edu.ng",
    tenant_id: "admin_01",
    tenant_name: "R A Omeiza",
    password: "multitenantmeteradmin",
};
let tenant_1_power_comand = false;
let tenant_2_power_comand = false;
let rechargeTenant1 = 0;
let rechargeTenant2 = 0;
const userSockets = new Map();
io.on('connection', (socket) => {
    console.log('A user connected:', socket.id);
    socket.on('disconnect', () => {
        console.log('A user disconnected:', socket.id);
    });
    socket.on('identify', (id) => {
        socket.join(id);
        userSockets.set(id, socket.id);
        console.log(`User ${socket.id} identified as ${id}`);
        io.to('tenant_1').emit('message', 'Hello, tenant_1!');
    });
    io.to('tenant_1').emit('message', 'Hello, tenant_1!');
});
app.get('/', (req, res) => {
    res.send({ message: 'Hello world' });
});
app.post('/user/login', (req, res) => {
    const { email, password } = req.body;
    console.log(req.body);
    if (email === tenant_1.email && password === tenant_1.password) {
        return res.send({ tenant: Object.assign(Object.assign({}, tenant_1), { password: null }) });
    }
    else if (email === tenant_2.email && password === tenant_2.password) {
        return res.send({ tenant: Object.assign(Object.assign({}, tenant_2), { password: null }) });
    }
    else if (email === admin.email && password === admin.password) {
        return res.send({ tenant: Object.assign(Object.assign({}, admin), { password: null }) });
    }
    else {
        return res.status(401).send({ error: 'Invalid email or password' });
    }
});
function sendMessageToUser(userId, message) {
    io.to(userId).emit('data', message);
}
app.get('/timestamp', (req, res) => {
    res.send({ timestamp: Date.now() });
});
app.post('/power', (req, res) => {
    if (req.body.tenant_id === 'tenant_01') {
        tenant_1_power_comand = true;
    }
    else if (req.body.tenant_id === 'tenant_02') {
        tenant_2_power_comand = true;
    }
    res.send({ success: true });
});
app.post('/admin/recharge', (req, res) => {
    const { tenantId, amount } = req.body;
    if (tenantId === 'tenant_01') {
        rechargeTenant1 = amount;
    }
    else if (tenantId === 'tenant_02') {
        rechargeTenant2 = amount;
    }
    else {
        return res.status(401).send({ error: 'Invalid tenant_id' });
    }
    res.send({ success: true });
});
app.post('/', (req, res) => {
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
exports.default = server;
