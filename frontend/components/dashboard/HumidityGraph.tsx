import { Line } from "react-chartjs-2";

import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
} from "chart.js";

ChartJS.register(
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  Title,
  Tooltip,
  Legend,
);

interface HumidityGraphProps {
  hourlyLabels: string[];
  humiditySeries: number[];
}

export default function HumidityGraph({
  hourlyLabels,
  humiditySeries,
}: HumidityGraphProps) {
  return (
    <div
      className="p-4 w-full h-full flex flex-col rounded-lg font-semibold"
      style={{ backgroundColor: "var(--secondary-color)" }}
    >
      <div className="text-base mb-2">Humidity Progression</div>
      <div className="flex-1 min-h-0">
        <div className="h-full">
          <Line
            data={{
              labels: hourlyLabels,
              datasets: [
                {
                  label: "% RH",
                  data: humiditySeries,
                  borderColor: "#4fc3f7",
                  backgroundColor: "rgba(79, 195, 247, 0.2)",
                  pointRadius: 3,
                  tension: 0.35,
                },
              ],
            }}
            options={{
              responsive: true,
              maintainAspectRatio: false,
              plugins: {
                legend: { display: false },
                tooltip: { mode: "index", intersect: false },
              },
              scales: {
                x: {
                  grid: { color: "rgba(255,255,255,0.08)" },
                  ticks: { color: "#bbb", maxRotation: 0, autoSkip: true },
                },
                y: {
                  grid: { color: "rgba(255,255,255,0.08)" },
                  ticks: { color: "#bbb" },
                },
              },
            }}
          />
        </div>
      </div>
    </div>
  );
}
