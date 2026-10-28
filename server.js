const express = require('express');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

// ให้บริการไฟล์ Static จากโฟลเดอร์ public
app.use(express.static(path.join(__dirname, 'public')));

const server = http.createServer(app);

// สร้าง WebSocket Server รับ Path /esp
const wss = new WebSocket.Server({ server, path: '/esp' });

wss.on('connection', (ws, req) => {
  const ip = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
  console.log(`[Server] Client connected from IP: ${ip}`);

  ws.on('message', (message) => {
    const dataStr = message.toString();
    console.log(`[Received]: ${dataStr}`);

    // ส่งข้อมูลต่อกระจายไปยัง Client ทั้งหมด (เช่น หน้าเว็บ Dashboard)
    wss.clients.forEach((client) => {
      if (client !== ws && client.readyState === WebSocket.OPEN) {
        client.send(dataStr);
      }
    });
  });

  ws.on('close', () => {
    console.log('[Server] Client disconnected');
  });

  ws.on('error', (err) => {
    console.error('[Server Error]:', err.message);
  });
});

// กำหนด '0.0.0.0' เพื่อให้ Render สแกนเจอ Port ทันทีที่สตาร์ท
server.listen(PORT, '0.0.0.0', () => {
  console.log(`[Server] Running on port ${PORT}`);
});