import GaugeComponent from "react-gauge-component";

interface AverageTemperatureProps {
  temperatureValue: number;
}

export default function AverageTemperature({
  temperatureValue,
}: AverageTemperatureProps) {
  return (
    <div
      className="p-4 w-full h-full flex flex-col justify-center items-center rounded-lg font-semibold"
      style={{ backgroundColor: "var(--secondary-color)" }}
    >
      <div className="text-base mb-2">Average Temperature</div>
      <div className="w-full max-w-[400px]">
        <GaugeComponent
          value={temperatureValue}
          type="grafana"
          minValue={0}
          maxValue={120}
          arc={{
            width: 0.25,
            subArcs: [
              { limit: 40, color: "#ffcc80" },
              { limit: 80, color: "#ff8a65" },
              { color: "#ac1d0b" },
            ],
          }}
          pointer={{
            type: "blob",
            color: "#d63814",
            elastic: true,
            maxFps: 30,
          }}
          labels={{
            valueLabel: {
              formatTextValue: (e) => "".concat(String(Math.round(e)), "°F"),
              style: {
                fontSize: "22px",
                fill: "#ff8a65",
                fontWeight: "bold",
              },
            },
            tickLabels: {
              type: "outer",
              ticks: [
                { value: 0 },
                { value: 10 },
                { value: 20 },
                { value: 30 },
                { value: 40 },
                { value: 50 },
                { value: 60 },
                { value: 70 },
                { value: 80 },
                { value: 90 },
                { value: 100 },
                { value: 110 },
                { value: 120 },
              ],
              defaultTickValueConfig: {
                formatTextValue: (e) => "".concat(String(e), "°"),
                style: { fontSize: "9px", fill: "#aaa" },
              },
            },
          }}
        />
      </div>
    </div>
  );
}
