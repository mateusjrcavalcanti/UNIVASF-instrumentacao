import { Progress } from "@/components/ui/progress";
import { cn } from "@/lib/utils";

export function Card({
  title,
  value,
  range,
  Icon,
  unity,
  color = "bg-green-400",
}: {
  title: string;
  value: number | string;
  unity?: string;
  range?: {
    max: number;
    min: number;
  };
  Icon: React.ComponentType<{ size: number; className?: string }>;
  color?: string;
}) {
  return (
    <div className="flex flex-col justify-center">
      <div className="w-56 p-4 bg-white shadow-lg rounded-2xl dark:bg-gray-800">
        <div className="flex items-center">
          <span className={cn("relative w-7 h-7 p-2", color, "rounded-full")}>
            <Icon
              size={20}
              className={cn(
                "absolute h-4",
                "text-white",
                "transform -translate-x-1/2 -translate-y-1/2 top-1/2 left-1/2"
              )}
            />
          </span>
          <p className="ml-2 text-gray-700 text-md dark:text-gray-50">
            {title}
          </p>
        </div>
        <div className="flex flex-col justify-start">
          <p className="mx-auto my-4 text-4xl font-bold text-left text-gray-800 dark:text-white">
            {value} {unity}
          </p>
        </div>
        {range && (
          <>
            <div className="relative h-2 bg-gray-200 rounded w-full">
              <Progress value={33} indicatorColor={color} />
            </div>
            <div className="flex mt-2 items-center text-sm justify-center">
              <span className="text-gray-600">{`${range.min} - ${range.max}`}</span>
            </div>
          </>
        )}
      </div>
    </div>
  );
}
