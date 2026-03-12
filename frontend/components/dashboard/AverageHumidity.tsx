import GaugeComponent from "react-gauge-component";

interface AverageHumidityProps {
  humidityValue: number;
}

export default function AverageHumidity({
  humidityValue,
}: AverageHumidityProps) {
  return (
    <div
      className="p-4 w-full h-full flex flex-col justify-center items-center rounded-lg font-semibold"
      style={{ backgroundColor: "var(--secondary-color)" }}
    >
      <div className="text-base mb-2">Average Humidity</div>

      <div className="w-full max-w-[400px]">
        <GaugeComponent
          value={humidityValue}
          type="grafana"
          arc={{
            width: 0.25,
            subArcs: [
              { limit: 30, color: "#ffcc80" },
              { limit: 60, color: "#4fc3f7" },
              { color: "#0277bd" },
            ],
          }}
          pointer={{
            type: "blob",
            color: "#4fc3f7",
            elastic: true,
            maxFps: 30,
          }}
          labels={{
            valueLabel: {
              formatTextValue: (e) => "".concat(String(Math.round(e)), "% RH"),
              style: {
                fontSize: "22px",
                fill: "#4fc3f7",
                fontWeight: "bold",
              },
            },
            tickLabels: {
              type: "outer",
              ticks: [
                { value: 0 },
                { value: 30 },
                { value: 60 },
                { value: 100 },
              ],
              defaultTickValueConfig: {
                formatTextValue: (e) => "".concat(String(e), "%"),
                style: { fontSize: "9px", fill: "#aaa" },
              },
            },
          }}
        />
      </div>
    </div>
  );
}
