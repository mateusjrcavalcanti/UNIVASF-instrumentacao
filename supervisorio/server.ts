import { parse } from "url";
import next from "next";
import { WebSocketServer, WebSocket } from "ws";
import http from "http";
import prisma from "./src/lib/db";

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

    ws.on("message", async (message) => {
      // Tenta converter a mensagem em um objeto JSON
      let messageObj;
      try {
        messageObj = JSON.parse(message.toString());
      } catch (error) {
        console.error("Mensagem inválida, não é um JSON válido.", error);
        return; // Sai da função se a mensagem não for um JSON válido
      }

      // Verifica se todas as propriedades esperadas estão presentes
      const expectedProperties = ["adc_reading", "voltage", "ldr_resistance"];
      const hasAllProperties = expectedProperties.every(
        (prop) => prop in messageObj
      );

      const { adc_reading, voltage, ldr_resistance } = messageObj;

      if (!hasAllProperties) {
        console.error(
          "A mensagem não contém todas as propriedades necessárias."
        );
        return; // Sai da função se a mensagem não tiver as propriedades esperadas
      }

      await prisma.data.create({
        data: {
          voltage: voltage,
          adc: adc_reading,
          resistance: ldr_resistance,
        },
      });

      // Broadcast message to all clients
      clients.forEach((client) => {
        if (client !== ws && client.readyState === WebSocket.OPEN) {
          client.send(message.toString());
        }
      });

      console.log(`Message received: ${message}`);
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
