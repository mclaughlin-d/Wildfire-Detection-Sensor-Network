/* eslint-disable @next/next/no-img-element */
"use client";
import { useEffect, useState } from "react";
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
        const readings = await fetchModuleReadings(selectedId, 10);
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
      <div className="grid grid-cols-1 grid-rows-3 gap-4 flex-1 min-h-0">
        <div
          className="w-full h-full flex flex-col rounded-lg font-semibold row-span-2 p-4 overflow-hidden"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          <div className="mb-4 text-red-500 justify-center items-center flex text-lg">
            FLAGGED SENSORS
          </div>
          <div className="flex-1 min-h-0 overflow-y-auto overflow-x-hidden space-y-3 pr-2">
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
          className="w-full h-full justify-center items-center flex rounded-lg font-semibold col-span-2 row-span-2 relative overflow-hidden"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          <TransformWrapper initialScale={1} minScale={0.5} maxScale={4}>
            <Controls />
            <TransformComponent
              wrapperStyle={{ width: "100%", height: "100%" }}
              contentStyle={{ width: "100%", height: "100%" }}
            >
              <div
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
          <div className="flex gap-3 h-full ">
            {selectedSensor ? (
              <div className="h-full flex flex-col gap-3 w-1/3">
                <div className="text-sm text-white text-xl border border-3 border-white rounded-xl p-4 flex justify-center">
                  {selectedSensor.id}
                </div>

                <div className="text-l text-gray-300 bg-[#5f6b70] rounded-xl flex pl-4 pt-2 pb-2 border-white ">
                  Latitude: {selectedSensor.latitude.toFixed(4)}
                  <br />
                  Longitude: {selectedSensor.longitude.toFixed(4)}
                </div>
                <div className="text-l text-gray-300 bg-[#5f6b70] rounded-xl flex pl-4 pt-2 pb-2 border-white">
                  {selectedSensor.flagged ? "Flagged" : "Not Flagged"}
                </div>
              </div>
            ) : (
              <div className="text-sm text-gray-300">
                Select a sensor module to view recent readings.
              </div>
            )}

            <div className="h-full w-full  bg-[#5f6b70] rounded-xl p-4">
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
                      <div className="overflow-y-auto space-y-2 pr-1">
                        {selectedReadings.map((reading) => (
                          <div
                            key={reading._id}
                            className="rounded-lg border border-gray-400/40 bg-[#4f5a5f] p-3 text-sm text-gray-200"
                          >
                            <div className="text-xs text-gray-300 mb-1">
                              {formatReadingTimestamp(reading.timestamp)}
                            </div>
                            <div>
                              Temp: {reading.temperature.toFixed(2)}°C |
                              Humidity: {reading.humidity.toFixed(2)}%
                            </div>
                            <div>
                              Gas: {reading.gas_raw} | Voltage:{" "}
                              {reading.pack_voltage.toFixed(2)}V
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
          </div>
        </div>
      </div>
    </div>
  );
}
