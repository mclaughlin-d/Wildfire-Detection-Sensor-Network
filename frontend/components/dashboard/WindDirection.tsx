import GaugeComponent from "react-gauge-component";

interface WindDirectionProps {
  windValue: number;
}

export default function WindDirection({ windValue }: WindDirectionProps) {
  const windDirectionLabel = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"][
    Math.round(windValue / 45) % 8
  ];

  return (
    <div
      className="p-4 w-full h-full flex flex-col justify-center items-center rounded-lg font-semibold"
      style={{ backgroundColor: "var(--secondary-color)" }}
    >
      <div className="text-base mb-2">Wind Direction</div>
      <div className="w-full max-w-[260px]">
        <GaugeComponent
          value={windValue}
          type="radial"
          minValue={0}
          maxValue={360}
          startAngle={-180}
          endAngle={180}
          arc={{
            width: 0.08,
            cornerRadius: 0,
            subArcs: [{ color: "#3498db" }],
          }}
          pointer={{
            type: "needle",
            color: "#e74c3c",
            length: 0.7,
            width: 6,
            maxFps: 30,
          }}
          labels={{
            valueLabel: {
              formatTextValue: (e) =>
                ""
                  .concat(String(Math.round(e)), "\xb0 ")
                  .concat(
                    ["N", "NE", "E", "SE", "S", "SW", "W", "NW"][
                      Math.round(e / 45) % 8
                    ],
                  ),
              style: {
                fontSize: "18px",
                fill: "#fff",
                fontWeight: "bold",
              },
            },
            tickLabels: {
              type: "outer",
              hideMinMax: true,
              ticks: [
                {
                  value: 0,
                  valueConfig: {
                    formatTextValue: () => "N",
                    style: {
                      fontSize: "12px",
                      fill: "#e74c3c",
                      fontWeight: "bold",
                    },
                  },
                },
                {
                  value: 90,
                  valueConfig: {
                    formatTextValue: () => "E",
                    style: { fontSize: "10px", fill: "#aaa" },
                  },
                },
                {
                  value: 180,
                  valueConfig: {
                    formatTextValue: () => "S",
                    style: { fontSize: "10px", fill: "#aaa" },
                  },
                },
                {
                  value: 270,
                  valueConfig: {
                    formatTextValue: () => "W",
                    style: { fontSize: "10px", fill: "#aaa" },
                  },
                },
              ],
              defaultTickLineConfig: { color: "#555", length: 4, width: 1 },
            },
          }}
        />
      </div>
      <div className="mt-2 text-sm font-normal text-gray-200">
        {windValue}° {windDirectionLabel}
      </div>
    </div>
  );
}
