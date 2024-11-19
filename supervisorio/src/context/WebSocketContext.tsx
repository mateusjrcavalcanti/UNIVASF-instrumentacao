"use client";
import React, { createContext, useContext, useEffect, useState } from "react";

export interface dataType {
  adc: number;
  adcPercent: number;
  voltage: number;
  resistance: number;
  pwm: number;
  pwmPercent: number;
  date: Date;
}
interface WebSocketContextType {
  sendMessage: (message: string) => void;
  messages: string[];
  data: dataType[];
}

const WebSocketContext = createContext<WebSocketContextType | undefined>(
  undefined
);

export const WebSocketProvider: React.FC<{ children: React.ReactNode }> = ({
  children,
}) => {
  const [webSocket, setWebSocket] = useState<WebSocket | null>(null);
  const [messages, setMessages] = useState<string[]>([]);
  const [data, setData] = useState<dataType[]>([]);

  useEffect(() => {
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:";
    const ws = new WebSocket(`${protocol}//${window.location.host}/api/ws`);

    ws.onopen = () => {
      console.log("Conexão estabelecida");
    };

    ws.onmessage = (event) => {
      if (event.data === "connection established") return;
      setMessages((prevMessages) => [...prevMessages, event.data]);
      const { adc_reading, voltage, ldr_resistance, pwm } = JSON.parse(
        event.data
      );
      setData((prevData) => [
        ...prevData,
        {
          adc: adc_reading,
          pwm,
          voltage,
          resistance: Number(ldr_resistance.toFixed(2)),
          adcPercent: Number(((100 * adc_reading) / 4095).toFixed(2)),
          pwmPercent: Number(((100 * pwm) / 8192).toFixed(2)),
          date: new Date(),
        },
      ]);
    };

    ws.onerror = (error) => {
      console.error("WebSocket error:", error);
    };

    ws.onclose = () => {
      console.log("WebSocket closed, tentarei reconectar...");
      setTimeout(() => {
        setWebSocket(
          new WebSocket(`${protocol}//${window.location.host}/api/ws`)
        );
      }, 5000);
    };

    setWebSocket(ws);

    return () => {
      ws.close();
    };
  }, []);

  const sendMessage = (message: string) => {
    if (webSocket && webSocket.readyState === WebSocket.OPEN) {
      webSocket.send(message);
    } else {
      console.error("WebSocket não está aberto");
    }
  };

  return (
    <WebSocketContext.Provider value={{ sendMessage, messages, data }}>
      {children}
    </WebSocketContext.Provider>
  );
};

export const useWebSocket = (): WebSocketContextType => {
  const context = useContext(WebSocketContext);
  if (!context) {
    throw new Error("useWebSocket must be used within a WebSocketProvider");
  }
  return context;
};
