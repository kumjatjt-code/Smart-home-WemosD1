const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const fs = require('fs');
const path = require('path');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server });

// กำหนดจำนวนช่องเป็น 8 ช่อง
const NUM_RELAYS = 8;
const STATE_FILE = path.join(__dirname, 'state_data.json');
let relayStates = new Array(NUM_RELAYS).fill(false);

// โหลดค่าสถานะเดิมจากไฟล์ state_data.json (ถ้ามี)
if (fs.existsSync(STATE_FILE)) {
    try {
        const data = JSON.parse(fs.readFileSync(STATE_FILE, 'utf8'));
        if (data.relays && Array.isArray(data.relays)) {
            for (let i = 0; i < NUM_RELAYS; i++) {
                relayStates[i] = data.relays[i] || false;
            }
        }
    } catch (e) {
        console.error("อ่านไฟล์สถานะไม่สำเร็จ", e);
    }
}

// ฟังก์ชันบันทึกสถานะลงไฟล์
function saveState() {
    fs.writeFileSync(STATE_FILE, JSON.stringify({ relays: relayStates }));
}

// ฟังก์ชันกระจายข้อความให้ทุกอุปกรณ์ที่เชื่อมต่ออยู่
function broadcast(msgObj) {
    const msgStr = JSON.stringify(msgObj);
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(msgStr);
        }
    });
}

// ให้ Express โหลดไฟล์หน้าเว็บจากโฟลเดอร์ public
app.use(express.static('public'));

wss.on('connection', (ws) => {
    console.log('Client connected');
    
    // ส่งสถานะล่าสุดให้ Client ทันทีที่เชื่อมต่อ
    ws.send(JSON.stringify({ type: 'INIT_STATE', relays: relayStates }));

    ws.on('message', (message) => {
        try {
            const doc = JSON.parse(message);
            
            if (doc.type === 'GET_STATE') {
                ws.send(JSON.stringify({ type: 'INIT_STATE', relays: relayStates }));
            } 
            else if (doc.type === 'CONTROL') {
                const ch = doc.ch;
                const state = doc.state;
                if (ch >= 0 && ch < NUM_RELAYS) {
                    relayStates[ch] = state;
                    saveState();
                    broadcast({ type: 'CONTROL', ch: ch, state: state, relays: relayStates });
                }
            } 
            else if (doc.type === 'CONTROL_ALL') {
                const state = doc.state;
                relayStates = new Array(NUM_RELAYS).fill(state);
                saveState();
                broadcast({ type: 'CONTROL_ALL', state: state, relays: relayStates });
            }
        } catch (e) {
            console.error("รูปแบบ JSON ไม่ถูกต้อง", e);
        }
    });

    ws.on('close', () => {
        console.log('Client disconnected');
    });
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
    console.log(`Server is running on port ${PORT}`);
});
