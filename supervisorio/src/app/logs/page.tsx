"use client";
import { useWebSocket } from "@/context/WebSocketContext";
import { useRef, useEffect } from "react";

export default function Home() {
  const { messages } = useWebSocket();
  const messagesEndRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    if (messagesEndRef.current) {
      messagesEndRef.current.scrollIntoView({ behavior: "smooth" });
    }
  }, [messages]);
  return (
    <div className="inline-flex">
      <div className="min-w-96 bg-white shadow-lg rounded-lg border border-gray-300 relative">
        <div className="bg-gray-200 h-8 flex items-center justify-between px-4">
          <div className="flex space-x-2">
            <div className="w-3 h-3 bg-red-500 rounded-full"></div>
            <div className="w-3 h-3 bg-yellow-500 rounded-full"></div>
            <div className="w-3 h-3 bg-green-500 rounded-full"></div>
          </div>
          <span className="text-gray-700">LOGS</span>
          <button className="text-gray-700 focus:outline-none">✕</button>
        </div>
        <div className="h-full max-h-96 overflow-auto px-6">
          <div className="text-gray-600">
            {messages.map((message, index) => (
              <p key={index}>{`${message}`}</p>
            ))}
          </div>
          <div ref={messagesEndRef} />
        </div>
      </div>
    </div>
  );
}
