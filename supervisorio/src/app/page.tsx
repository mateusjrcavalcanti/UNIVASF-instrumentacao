"use client";
import { MdElectricBolt } from "react-icons/md";
import { InfoCard } from "./info-card";
import { FaWaveSquare } from "react-icons/fa";
import { GiElectricalResistance } from "react-icons/gi";
import { GoNumber } from "react-icons/go";
import { useWebSocket } from "@/context/WebSocketContext";

import { Chart } from "./chart";

export default function Home() {
  const { data } = useWebSocket();

  const { voltage, resistance, adc, adcPercent, pwm, pwmPercent } = data.length
    ? data[data.length - 1]
    : {
        voltage: 0,
        resistance: 0,
        adc: 0,
        adcPercent: 0,
        pwm: 0,
        pwmPercent: 0,
      };

  return (
    <>
      <div className="flex justify-between px-4 pb-4">
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
      <div className="flex-grow px-4">
        <Chart data={data} />
      </div>
    </>
  );
}
