import { MdElectricBolt } from "react-icons/md";
import { Card } from "./card";
import { FaWaveSquare } from "react-icons/fa";
import { GiElectricalResistance } from "react-icons/gi";
import { GoNumber } from "react-icons/go";

export default function Home() {
  return (
    <>
      <div className="flex flex-wrap justify-between px-4 pb-4">
        <Card
          title={"PWM"}
          value={100}
          Icon={FaWaveSquare}
          unity="%"
          color="bg-blue-500"
        />

        <Card
          title={"ADC"}
          value={100}
          unity="%"
          Icon={GoNumber}
          color="bg-red-500"
        />

        <Card
          title={"Tensão"}
          value={1.5}
          unity="V"
          Icon={MdElectricBolt}
          color="bg-orange-400"
        />

        <Card
          title={"Resistência"}
          value={300}
          unity="Ω"
          Icon={GiElectricalResistance}
          color="bg-yellow-500"
        />

        <Card
          title={"PWM"}
          value={100}
          Icon={FaWaveSquare}
          range={{ min: 0, max: 4095 }}
          color="bg-blue-500"
        />

        <Card
          title={"ADC"}
          value={100}
          Icon={GoNumber}
          range={{ min: 0, max: 4095 }}
          color="bg-red-500"
        />
      </div>
      <div className="w-full mt-4">
        <div className="mx-0 mb-4 sm:ml-4 xl:mr-4">
          <div className="w-full h-96 bg-white shadow-lg rounded-2xl dark:bg-gray-800"></div>
        </div>
      </div>
    </>
  );
}
