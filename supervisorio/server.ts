import { parse } from "url";
import next from "next";
import { WebSocketServer, WebSocket } from "ws";
import http from "http";

const nextApp = next({ dev: process.env.NODE_ENV !== "production" });
const clients: Set<WebSocket> = new Set();

nextApp.prepare().then(() => {
  const server = http.createServer((req, res) => {
    const handle = nextApp.getRequestHandler();
    handle(req, res);
  });

  const wss = new WebSocketServer({ noServer: true });

  wss.on("connection", (ws: WebSocket) => {
    clients.add(ws);
    console.log("New client connected");

    ws.on("message", (message) => {
      console.log(`Message received: ${message}`);
      // Broadcast message to all clients
      clients.forEach((client) => {
        if (client !== ws && client.readyState === WebSocket.OPEN) {
          client.send(message.toString());
        }
      });
    });

    ws.on("close", () => {
      clients.delete(ws);
      console.log("Client disconnected");
    });
  });

  server.on("upgrade", (req, socket, head) => {
    const { pathname } = parse(req.url || "/", true);

    // Hot module reloading
    if (pathname === "/_next/webpack-hmr") {
      nextApp.getUpgradeHandler()(req, socket, head);
    }

    // Upgrade to WebSocket
    if (pathname === "/api/ws") {
      wss.handleUpgrade(req, socket, head, (ws: WebSocket) => {
        wss.emit("connection", ws, req);
      });
    }
  });

  const PORT = 3000;
  server.listen(PORT, () => {
    console.log(`Server is running on http://localhost:${PORT}`);
  });
});
