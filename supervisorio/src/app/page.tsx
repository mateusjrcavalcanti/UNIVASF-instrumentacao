"use client";
import { MdElectricBolt } from "react-icons/md";
import { InfoCard } from "./info-card";
import { FaWaveSquare } from "react-icons/fa";
import { GiElectricalResistance } from "react-icons/gi";
import { GoNumber } from "react-icons/go";
import { useWebSocket } from "@/context/WebSocketContext";
import { Chart } from "./chart";
import { useState } from "react";
import axios from "axios";

export default function Home() {
  const { data: liveData } = useWebSocket();

  const [historicalData, setHistoricalData] = useState([]);
  const [startDate, setStartDate] = useState("");
  const [endDate, setEndDate] = useState("");

  // Dados combinados
  const combinedData = [...historicalData, ...liveData];
  // const combinedData = [...liveData];

  const { voltage, resistance, adc, adcPercent, pwm, pwmPercent } =
    combinedData.length
      ? combinedData[combinedData.length - 1]
      : {
          voltage: 0,
          resistance: 0,
          adc: 0,
          adcPercent: 0,
          pwm: 0,
          pwmPercent: 0,
        };

  // Função para buscar dados históricos
  const fetchHistoricalData = async () => {
    try {
      const response = await axios.get("/api/data", {
        params: {
          startDate: `${startDate}T00:00:00Z`,
          endDate: `${endDate}T23:59:59Z`,
        },
      });

      const data = response.data.map((item) => ({
        adc: item.adc,
        pwm: item.pwm,
        date: new Date(item.createdAt),
        resistance: Number(item.resistance.toFixed(2)),
        adcPercent: Number(((100 * item.adc) / 4095).toFixed(2)),
        pwmPercent: Number(((100 * pwm) / 8192).toFixed(2)),
      }));

      setHistoricalData(data);
    } catch (error) {
      console.error("Erro ao buscar dados históricos:", error);
    }
  };

  return (
    <>
      <div className="flex flex-col px-4 pb-4">
        {/* Cards de informações */}
        <div className="flex justify-between">
          <InfoCard
            title={"PWM"}
            value={pwmPercent}
            Icon={FaWaveSquare}
            unity="%"
            color="bg-blue-500"
          />
          <InfoCard
            title={"ADC"}
            value={adcPercent}
            unity="%"
            Icon={GoNumber}
            color="bg-red-500"
          />
          <InfoCard
            title={"Tensão"}
            value={voltage}
            unity="V"
            Icon={MdElectricBolt}
            color="bg-orange-400"
          />
          <InfoCard
            title={"Resistência"}
            value={resistance}
            unity="Ω"
            Icon={GiElectricalResistance}
            color="bg-yellow-500"
          />
          <InfoCard
            title={"PWM"}
            value={pwm}
            Icon={FaWaveSquare}
            range={{ min: 0, max: 8192 }}
            color="bg-blue-500"
          />
          <InfoCard
            title={"ADC"}
            value={adc}
            Icon={GoNumber}
            range={{ min: 0, max: 4095 }}
            color="bg-red-500"
          />
        </div>
      </div>

      {/* Formulário para seleção de intervalo */}
      <form
        className="flex justify-between items-end gap-4 mb-4 w-4/12"
        onSubmit={(e) => {
          e.preventDefault();
          fetchHistoricalData();
        }}
      >
        <div>
          <label className="block text-sm font-medium text-gray-700">
            Data Inicial
          </label>
          <input
            type="date"
            value={startDate}
            onChange={(e) => setStartDate(e.target.value)}
            className="mt-1 block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-yellow-500 focus:border-yellow-500 sm:text-sm"
          />
        </div>
        <div>
          <label className="block text-sm font-medium text-gray-700">
            Data Final
          </label>
          <input
            type="date"
            value={endDate}
            onChange={(e) => setEndDate(e.target.value)}
            className="mt-1 block w-full px-3 py-2 border border-gray-300 rounded-md shadow-sm focus:outline-none focus:ring-yellow-500 focus:border-yellow-500 sm:text-sm"
          />
        </div>
        <button
          type="submit"
          className="px-4 py-2 bg-yellow-500 text-white rounded-md shadow-sm hover:bg-yellow-600"
        >
          Consultar
        </button>
      </form>

      {/* Gráfico */}
      <div className="flex-grow px-4">
        <Chart data={combinedData} />
      </div>
    </>
  );
}
