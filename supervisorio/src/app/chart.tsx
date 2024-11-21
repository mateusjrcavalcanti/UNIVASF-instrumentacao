import { CartesianGrid, Line, LineChart, XAxis, YAxis } from "recharts";
import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import {
  ChartConfig,
  ChartContainer,
  ChartTooltip,
  ChartTooltipContent,
} from "@/components/ui/chart";
import { format } from "date-fns";
import type { dataType } from "@/context/WebSocketContext";

const formattedData = (data: dataType[]) => {
  return data.map((item) => ({
    ...item,
    formattedDate: item.date ? format(item.date, "HH:mm:ss") : "Invalid Date",
  }));
};

export function Chart({ data }: { data: dataType[] }) {
  const chartConfig = {
    adc: {
      label: "ADC",
      color: "hsl(var(--chart-1))",
    },
    resistance: {
      label: "Resistance",
      color: "hsl(var(--chart-2))",
    },
    voltage: {
      label: "Voltage",
      color: "hsl(var(--chart-3))",
    },
  } satisfies ChartConfig;

  if (!data || data.length === 0) {
    return <p>No data available for chart.</p>;
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle>Gráfico</CardTitle>
        <CardDescription>Tensão, Resistência...</CardDescription>
      </CardHeader>
      <CardContent>
        <ChartContainer config={chartConfig} className="h-96 w-full">
          <LineChart
            data={formattedData(data)}
            margin={{ left: 12, right: 12 }}
          >
            <CartesianGrid vertical={false} />
            <XAxis dataKey="formattedDate" />
            <YAxis />
            <ChartTooltip cursor={false} content={<ChartTooltipContent />} />
            <Line
              dataKey="adc"
              type="linear"
              stroke={chartConfig.adc.color}
              strokeWidth={2}
              dot={false}
              isAnimationActive={false}
            />
          </LineChart>
        </ChartContainer>
      </CardContent>
      <CardFooter></CardFooter>
    </Card>
  );
}
