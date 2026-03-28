/* eslint-disable @next/next/no-img-element */
"use client";
import { useEffect, useRef, useState } from "react";
import { FaLocationDot } from "react-icons/fa6";
import {
  TransformWrapper,
  TransformComponent,
  useControls,
} from "react-zoom-pan-pinch";
import {
  fetchModuleReadings,
  fetchModules,
  type ModuleReading,
  type ModuleSummary,
} from "@/lib/api";

const formatReadingTimestamp = (timestamp: string) =>
  new Date(timestamp).toLocaleString("en-US", {
    month: "short",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false,
  });

// Reinterpret uint16 bits as IEEE 754 float16, return float32
function float16ToFloat32(u16: number): number {
  const sign = (u16 >> 15) & 1;
  const exp = (u16 >> 10) & 0x1f;
  const frac = u16 & 0x3ff;
  if (exp === 0) return (sign ? -1 : 1) * Math.pow(2, -14) * (frac / 1024);
  if (exp === 0x1f) return frac ? NaN : sign ? -Infinity : Infinity;
  return (sign ? -1 : 1) * Math.pow(2, exp - 15) * (1 + frac / 1024);
}

// Jet colormap: t in [0,1] -> [r,g,b]
function jetColor(t: number): [number, number, number] {
  const clamp = (v: number) => Math.max(0, Math.min(255, Math.round(v * 255)));
  return [
    clamp(1.5 - Math.abs(4 * t - 3)),
    clamp(1.5 - Math.abs(4 * t - 2)),
    clamp(1.5 - Math.abs(4 * t - 1)),
  ];
}

function ThermalCanvas({
  readings,
}: {
  readings: import("@/lib/api").ModuleReading[];
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    // Build 4 frames, each 24 rows × 32 cols, filled with zeros
    const frames: number[][][] = Array.from({ length: 4 }, () =>
      Array.from({ length: 24 }, () => new Array(32).fill(0)),
    );
    const filled: boolean[][] = Array.from({ length: 4 }, () =>
      new Array(8).fill(false),
    );

    // Fill from readings (most recent first — readings are ordered newest first)
    for (const r of readings) {
      if (!r.payload || r.payload.length < 96) continue;
      if (r.sequence_num === null || r.row_sequence === null) continue;
      const seq = r.sequence_num;
      const row = r.row_sequence;
      if (seq < 0 || seq > 3 || row < 0 || row > 7) continue;
      if (filled[3 - seq][row]) continue;
      filled[3 - seq][row] = true;
      for (let i = 0; i < 3; i++) {
        console.log(r.payload)
        console.log(frames)
        frames[3 - seq][row * 3 + i] = r.payload.slice(i * 32, i * 32 + 32);
        frames[3 - seq][row * 3 + i].reverse()
      }
    }

    // Apply firmware transform: np.rot90(np.flip(frame)) = frame[:, ::-1].T
    // Input: (24, 32) → Output: (32, 24)   result[i][j] = frame[j][31 - i]
    const transformed = frames.map((frame) =>
      Array.from({ length: 32 }, (_, i) =>
        Array.from({ length: 24 }, (_, j) => frame[j][i]),
      ),
    );

    // Combine: hstack(frame3, frame2, frame1, frame0) → 32 rows × 96 cols
    const combined: number[] = [];
    for (let i = 0; i < 32; i++) {
      combined.push(
        ...transformed[3][i],
        ...transformed[2][i],
        ...transformed[1][i],
        ...transformed[0][i],
      );
    }

    const floatData = combined.map(float16ToFloat32);
    const valid = floatData.filter(isFinite);
    if (valid.length === 0) {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      return;
    }
    const minVal = Math.min(...valid);
    const maxVal = Math.max(...valid);
    const range = maxVal - minVal || 1;

    // Apply fliplr: reverse each row (matching firmware np.fliplr)
    const dataArray: number[][] = [];
    for (let row = 31; row >= 0; row--) {
      const rowData: number[] = [];
      for (let col = 0; col < 96; col++) {
        rowData.push(floatData[row * 96 + col]);
      }
      dataArray.push(rowData);
    }

    const SCALE = 5;
    const W = 96 * SCALE;
    const H = 32 * SCALE;
    canvas.width = W;
    canvas.height = H;
    const imageData = ctx.createImageData(W, H);

    for (let row = 0; row < 32; row++) {
      for (let col = 0; col < 96; col++) {
        const v = dataArray[row][col];
        const t = isFinite(v) ? (v - minVal) / range : 0;
        const [r, g, b] = jetColor(t);
        for (let dy = 0; dy < SCALE; dy++) {
          for (let dx = 0; dx < SCALE; dx++) {
            const px = ((row * SCALE + dy) * W + (col * SCALE + dx)) * 4;
            imageData.data[px] = r;
            imageData.data[px + 1] = g;
            imageData.data[px + 2] = b;
            imageData.data[px + 3] = 255;
          }
        }
      }
    }
    ctx.putImageData(imageData, 0, 0);
  }, [readings]);

  return (
    <canvas
      ref={canvasRef}
      style={{ width: "100%", height: "auto", imageRendering: "pixelated" }}
    />
  );
}

export default function MapView() {
  const [modules, setModules] = useState<ModuleSummary[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [selectedReadings, setSelectedReadings] = useState<ModuleReading[]>([]);
  const [isReadingsLoading, setIsReadingsLoading] = useState(false);
  const [readingsError, setReadingsError] = useState<string | null>(null);

  useEffect(() => {
    let isMounted = true;

    const loadModules = async () => {
      try {
        const data = await fetchModules();
        if (isMounted) {
          setModules(data);
          setError(null);
        }
      } catch (err) {
        if (isMounted) {
          setError(err instanceof Error ? err.message : "Unknown error");
        }
      } finally {
        if (isMounted) {
          setIsLoading(false);
        }
      }
    };

    loadModules();

    return () => {
      isMounted = false;
    };
  }, []);

  const flaggedSensors = modules.filter((sensor) => sensor.flagged);

  const [selectedId, setSelectedId] = useState<string | undefined>(undefined);

  useEffect(() => {
    if (!selectedId && flaggedSensors[0]) {
      setSelectedId(flaggedSensors[0].id);
    }
  }, [flaggedSensors, selectedId]);

  const selectedSensor = modules.find((sensor) => sensor.id === selectedId);
  const thermalReadings = selectedReadings
    .filter((reading) => reading.payload && reading.payload.length >= 96)
    .slice(0, 40);

  useEffect(() => {
    if (!selectedId) {
      setSelectedReadings([]);
      setReadingsError(null);
      return;
    }

    let isMounted = true;

    const loadSelectedReadings = async () => {
      setIsReadingsLoading(true);
      try {
        const readings = await fetchModuleReadings(selectedId, 100);
        if (isMounted) {
          setSelectedReadings(readings);
          setReadingsError(null);
        }
      } catch (err) {
        if (isMounted) {
          setReadingsError(
            err instanceof Error ? err.message : "Unknown error",
          );
          setSelectedReadings([]);
        }
      } finally {
        if (isMounted) {
          setIsReadingsLoading(false);
        }
      }
    };

    loadSelectedReadings();
    const interval = setInterval(loadSelectedReadings, 10000);

    return () => {
      isMounted = false;
      clearInterval(interval);
    };
  }, [selectedId]);

  //convert lat and long to positions on the "map" aka an image for now
  const getModulePosition = (module: ModuleSummary) => {
    //find lat and long bounds
    const lats = modules.map((m) => m.latitude);
    const lngs = modules.map((m) => m.longitude);
    const minLat = Math.min(...lats);
    const maxLat = Math.max(...lats);
    const minLng = Math.min(...lngs);
    const maxLng = Math.max(...lngs);

    const latRange = maxLat - minLat || 1;
    const lngRange = maxLng - minLng || 1;

    const x = 10 + ((module.longitude - minLng) / lngRange) * 80;
    const y = 10 + ((maxLat - module.latitude) / latRange) * 80; // Inverted for screen coords

    return { x, y };
  };

  const Controls = () => {
    const { zoomIn, zoomOut, resetTransform } = useControls();
    return (
      <div className="absolute top-4 right-4 z-10 flex gap-2 mr-2">
        <button
          onClick={() => zoomIn()}
          className="bg-white/50 hover:bg-white text-gray-800 font-bold py-2 px-4 rounded shadow"
        >
          +
        </button>
        <button
          onClick={() => zoomOut()}
          className="bg-white/50 hover:bg-white text-gray-800 font-bold py-2 px-4 rounded shadow min-w-[42px]"
        >
          -
        </button>
        <button
          onClick={() => resetTransform()}
          className="bg-white/50 hover:bg-white text-gray-800 font-bold py-2 px-4 rounded shadow min-w-[42px]"
        >
          Reset
        </button>
      </div>
    );
  };

  return (
    <div className="ml-30 mr-4 mt-4 mb-4 h-screen flex flex-col p-4">
      <div className="grid grid-cols-1 grid-rows-[minmax(0,3fr)_minmax(0,2fr)] gap-4 flex-1 min-h-0">
        <div
          className="w-full h-full flex flex-col rounded-lg font-semibold row-span-1 p-4 overflow-hidden"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          <div className="mb-4 text-red-500 justify-center items-center flex text-lg">
            FLAGGED SENSORS
          </div>
          <div className="transparent-scrollbar flex-1 min-h-0 overflow-y-auto overflow-x-hidden space-y-3 pr-2">
            {isLoading && (
              <div className="text-sm text-gray-300">Loading modules...</div>
            )}
            {!isLoading && error && (
              <div className="text-sm text-red-300">{error}</div>
            )}
            {!isLoading &&
              !error &&
              flaggedSensors.map((sensor) => (
                <button
                  key={sensor.id}
                  type="button"
                  onClick={() => setSelectedId(sensor.id)}
                  className={`w-full rounded-2xl border px-4 py-4 text-left transition shadow-sm ${
                    selectedId === sensor.id
                      ? "border-white border-2 bg-[#5f6b70] hover:bg-[#667279] opacity-100"
                      : "border-red-500 bg-[#5f6b70] hover:bg-[#667279] opacity-70 "
                  }`}
                >
                  <div className="flex items-center gap-4">
                    <div className="flex h-12 w-12 items-center justify-center rounded-full border-2 border-red-500 text-red-500">
                      <FaLocationDot className="text-2xl" />
                    </div>
                    <div>
                      <div className="text-lg font-semibold text-gray-100">
                        {sensor.id}
                      </div>
                      <div className="text-sm text-gray-300">
                        {sensor.latitude.toFixed(4)}°N,{" "}
                        {Math.abs(sensor.longitude).toFixed(4)}°W
                      </div>
                    </div>
                  </div>
                </button>
              ))}
            {!isLoading && !error && flaggedSensors.length === 0 && (
              <div className="text-sm text-gray-300">No flagged sensors.</div>
            )}
          </div>
        </div>
        <div
          className="w-full h-full justify-center items-center flex rounded-lg font-semibold col-span-2 row-span-1 relative overflow-hidden"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          <TransformWrapper initialScale={1} minScale={0.5} maxScale={4}>
            <Controls />
            <TransformComponent
              wrapperStyle={{ width: "100%", height: "100%" }}
              contentStyle={{ width: "100%", height: "100%" }}
            >
              <div
                className="overflow-hidden rounded-lg"
                style={{ position: "relative", width: "100%", height: "100%" }}
              >
                <img
                  src="/map-view/stock-map-image.png"
                  alt="Forest Map"
                  style={{ width: "100%", height: "100%", objectFit: "cover" }}
                />

                {/* Module markers */}
                {!isLoading &&
                  !error &&
                  modules.map((module) => {
                    const { x, y } = getModulePosition(module);
                    const isFlagged = module.flagged;
                    const isSelected = selectedId === module.id;

                    return (
                      <button
                        key={module.id}
                        onClick={() => setSelectedId(module.id)}
                        style={{
                          position: "absolute",
                          left: `${x}%`,
                          top: `${y}%`,
                          transform: "translate(-50%, -50%)",
                        }}
                        className={`
                        w-8 h-8 rounded-full cursor-pointer
                        transition-all duration-200
                        
                        ${isSelected ? "scale-125 ring-4 ring-white" : "hover:scale-110"}
                        ${
                          isFlagged
                            ? "bg-red-500 border-red-700 hover:bg-red-400"
                            : "bg-green-500 border-green-700 hover:bg-green-400"
                        }
                      `}
                        title={`${module.id}${isFlagged ? " - FLAGGED" : ""}`}
                      >
                        <div className="text-white text-xs font-bold">
                          {module.id.split("-")[1]}
                        </div>
                      </button>
                    );
                  })}
              </div>
            </TransformComponent>
          </TransformWrapper>
        </div>

        <div
          className="w-full h-full flex flex-col rounded-lg font-semibold col-span-3 p-4"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          <div className="flex gap-3 h-full">
            {/* Column 1: Module info */}
            <div className="transparent-scrollbar h-full min-h-0 overflow-y-auto flex flex-col gap-3 w-1/5 shrink-0 pr-1">
              {selectedSensor ? (
                <>
                  <div
                    className={`text-white border border-3 rounded-xl p-4 flex flex-col items-center text-center ${selectedSensor.flagged ? "border-red-500" : "border-green-500"}`}
                  >
                    <div className="text-xl font-semibold">
                      {selectedSensor.id}
                    </div>
                    <div className="text-sm text-gray-300 mt-1">
                      {selectedSensor.latitude.toFixed(4)}°N,{" "}
                      {Math.abs(selectedSensor.longitude).toFixed(4)}°W
                    </div>
                  </div>

                  <div
                    className={`text-sm text-gray-300 rounded-xl pl-3 pt-2 pb-2 border-white leading-tight ${selectedSensor.flagged ? "bg-red-500/20" : "bg-green-500/20"}`}
                  >
                    <div>
                      Confidence Reading:{" "}
                      {selectedReadings[0]?.fire_confidence == null
                        ? "N/A"
                        : selectedReadings[0].fire_confidence.toFixed(3)}
                    </div>
                  </div>

                  <div
                    className={`text-sm text-gray-300 rounded-xl pl-3 pt-2 pb-2 border-white leading-tight ${selectedReadings[0]?.pack_voltage != null && selectedReadings[0].pack_voltage < 10.75 ? "bg-red-500/20" : "bg-green-500/20"}`}
                  >
                    <div>
                      Pack Voltage:{" "}
                      <span className="font-semibold">
                        {selectedReadings[0]?.pack_voltage == null
                          ? "N/A"
                          : `${selectedReadings[0].pack_voltage.toFixed(3)}V`}
                      </span>
                    </div>
                  </div>

                  <div className="text-sm text-gray-300 bg-[#5f6b70] rounded-xl p-2  leading-tight">
                    <div className="text-sm text-gray-300">
                      Solar Voltage: 15.1V
                    </div>
                    <div className="text-sm text-gray-300">
                      Solar Current: 1.78A
                    </div>
                    <div className="text-sm text-gray-300">
                      Manual Power: 0.00V
                    </div>
                    <div className="text-sm text-gray-300">
                      Battery 1: 3.74V
                    </div>
                    <div className="text-sm text-gray-300">
                      Battery 2: 3.69V
                    </div>
                    <div className="text-sm text-gray-300">
                      Battery 3: 3.81V
                    </div>
                    <div className="text-sm text-gray-300">
                      Battery Output Current: 0.52A
                    </div>
                  </div>
                </>
              ) : (
                <div className="text-sm text-gray-300">
                  Select a sensor module to view recent readings.
                </div>
              )}
            </div>

            {/* Column 2: Recent readings */}
            <div className="h-full w-1/3 shrink-0 bg-[#5f6b70] rounded-xl p-4">
              {selectedSensor ? (
                <div className="h-full flex flex-col">
                  <div className="text-white text-xl mb-3">Recent readings</div>

                  {isReadingsLoading && (
                    <div className="text-sm text-gray-300">
                      Loading readings...
                    </div>
                  )}

                  {!isReadingsLoading && readingsError && (
                    <div className="text-sm text-red-300">{readingsError}</div>
                  )}

                  {!isReadingsLoading &&
                    !readingsError &&
                    selectedReadings.length === 0 && (
                      <div className="text-sm text-gray-300">
                        No readings available for this module.
                      </div>
                    )}

                  {!isReadingsLoading &&
                    !readingsError &&
                    selectedReadings.length > 0 && (
                      <div className="transparent-scrollbar overflow-y-auto space-y-2 pr-1">
                        {selectedReadings.slice(0, 10).map((reading) => (
                          <div
                            key={reading._id}
                            className="rounded-lg border border-gray-400/40 bg-[#4f5a5f] p-3 text-sm text-gray-200"
                          >
                            <div className="text-xs text-gray-300 mb-1">
                              {formatReadingTimestamp(reading.timestamp)}
                            </div>
                            <div>
                              Temp: {reading.temperature.toFixed(2)}°C |
                              Humidity: {reading.humidity.toFixed(2)}% | Gas:{" "}
                              {reading.gas_sensor.toFixed(2)}
                            </div>
                          </div>
                        ))}
                      </div>
                    )}
                </div>
              ) : (
                <div className="text-sm text-gray-300">No sensor selected.</div>
              )}
            </div>

            {/* Column 3: Thermal image */}
            <div className="h-full flex-1 bg-[#5f6b70] rounded-xl p-4 flex flex-col min-w-0">
              <div className="flex justify-between items-center mb-3">
                <div className="text-white text-xl">Recent Thermal Image</div>
                {selectedSensor && thermalReadings.length > 0 && (
                  <div className="text-sm text-gray-400">
                    {formatReadingTimestamp(
                      thermalReadings[0]?.timestamp || "",
                    )}
                  </div>
                )}
              </div>
              {selectedSensor ? (
                <div className="flex-1 flex flex-col items-center justify-center min-h-0 overflow-hidden">
                  {thermalReadings.length > 0 ? (
                    <ThermalCanvas readings={thermalReadings} />
                  ) : (
                    <div className="text-sm text-gray-300">
                      {isReadingsLoading
                        ? "Loading..."
                        : "No thermal data available."}
                    </div>
                  )}
                </div>
              ) : (
                <div className="text-sm text-gray-300">No sensor selected.</div>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
