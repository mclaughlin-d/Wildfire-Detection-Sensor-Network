"use client";
import { useEffect, useState } from "react";

//home page tiles
import DashboardSelectionPanel from "@/components/dashboard/DashboardSelectionPanel";
import WindDirection from "@/components/dashboard/WindDirection";
import TemperatureGraph from "@/components/dashboard/TemperatureGraph";
import AverageTemperature from "@/components/dashboard/AverageTemperature";
import FlaggedSensors from "@/components/dashboard/FlaggedSensors";
import HumidityGraph from "@/components/dashboard/HumidityGraph";
import AverageHumidity from "@/components/dashboard/AverageHumidity";
import {
  fetchModuleReadings,
  fetchModules,
  type ModuleReading,
} from "@/lib/api";

const celsiusToFahrenheit = (celsius: number) => (celsius * 9) / 5 + 32;

const formatTimeLabel = (timestamp: string) =>
  new Date(timestamp).toLocaleTimeString("en-US", {
    hour: "2-digit",
    minute: "2-digit",
    hour12: false,
  });

export default function Home() {
  const [time, setTime] = useState<string>("");
  const [date, setDate] = useState<string>("");
  const [temperatureValue, setTemperatureValue] = useState<number>(72);
  const [windValue, setWindValue] = useState<number>(167);
  const [humidityValue, setHumidityValue] = useState<number>(70);
  const [hasFlaggedSensors, setHasFlaggedSensors] = useState<boolean>(false);
  const [hourlyLabels, setHourlyLabels] = useState<string[]>([]);
  const [temperatureSeries, setTemperatureSeries] = useState<number[]>([]);
  const [humiditySeries, setHumiditySeries] = useState<number[]>([]);

  //initialize visibleComponents from local storage (so it persists among pages)
  const [visibleComponents, setVisibleComponents] = useState<string[]>(() => {
    if (typeof window !== "undefined") {
      const saved = localStorage.getItem("dashboardLayout");
      if (saved) {
        return JSON.parse(saved);
      }
    }
    //default
    return [
      "windDirection",
      "temperatureGraph",
      "averageTemperature",
      "flaggedSensors",
      "humidityGraph",
      "averageHumidity",
    ];
  });

  // Save to localStorage whenever visibleComponents changes
  useEffect(() => {
    if (visibleComponents.length > 0) {
      localStorage.setItem(
        "dashboardLayout",
        JSON.stringify(visibleComponents),
      );
    }
  }, [visibleComponents]);

  //set up clock
  useEffect(() => {
    let isMounted = true;

    const loadDashboardData = async () => {
      try {
        const modules = await fetchModules();
        if (!isMounted) {
          return;
        }

        setHasFlaggedSensors(modules.some((module) => module.flagged));

        const readingsPerModule = await Promise.all(
          modules.map(async (module) => {
            try {
              return await fetchModuleReadings(module.id, 12);
            } catch {
              return [] as ModuleReading[];
            }
          }),
        );

        if (!isMounted) {
          return;
        }

        const latestReadings = readingsPerModule
          .map((readings) => readings[0])
          .filter(Boolean) as ModuleReading[];

        if (latestReadings.length > 0) {
          const avgTempF =
            latestReadings.reduce(
              (sum, reading) => sum + celsiusToFahrenheit(reading.temperature),
              0,
            ) / latestReadings.length;
          const avgHumidity =
            latestReadings.reduce((sum, reading) => sum + reading.humidity, 0) /
            latestReadings.length;
          const avgGasSensor =
            latestReadings.reduce(
              (sum, reading) => sum + reading.gas_sensor,
              0,
            ) / latestReadings.length;

          setTemperatureValue(Math.round(avgTempF));
          setHumidityValue(Math.round(avgHumidity));
          setWindValue(Math.round(avgGasSensor) % 360);
        }

        const combinedReadings = readingsPerModule
          .flat()
          .sort(
            (first, second) =>
              new Date(first.timestamp).getTime() -
              new Date(second.timestamp).getTime(),
          )
          .slice(-12);

        if (combinedReadings.length > 0) {
          setHourlyLabels(
            combinedReadings.map((reading) =>
              formatTimeLabel(reading.timestamp),
            ),
          );
          setTemperatureSeries(
            combinedReadings.map((reading) =>
              Math.round(celsiusToFahrenheit(reading.temperature)),
            ),
          );
          setHumiditySeries(
            combinedReadings.map((reading) => Math.round(reading.humidity)),
          );
        }
      } catch {}
    };

    loadDashboardData();
    const interval = setInterval(loadDashboardData, 10000);

    return () => {
      isMounted = false;
      clearInterval(interval);
    };
  }, []);

  useEffect(() => {
    const updateDateTime = () => {
      const now = new Date();
      setTime(
        now.toLocaleTimeString("en-US", {
          hour: "2-digit",
          minute: "2-digit",
          hour12: false,
        }),
      );
      setDate(
        now.toLocaleDateString("en-US", {
          year: "numeric",
          month: "long",
          day: "numeric",
        }),
      );
    };

    updateDateTime();

    const interval = setInterval(updateDateTime, 60000); //sets timer, update once a minute
    return () => clearInterval(interval); //clears timer on unmount
  }, []);

  return (
    <div className="ml-30 mr-10 h-screen flex flex-col p-4 pb-28">
      {/* Customize Dashboard Panel */}
      <DashboardSelectionPanel
        visibleComponents={visibleComponents}
        setVisibleComponents={setVisibleComponents}
      />

      <div className="text-right mb-4 mt-4">
        <h1 className="text-2xl">{time}</h1>
        <h1 className="text-2xl">{date}</h1>
      </div>

      <div className="grid grid-cols-3 grid-rows-2 gap-4 flex-1">
        {/*render components based on what is visible (selections changed through selection panel)*/}
        {visibleComponents.includes("windDirection") && (
          <WindDirection windValue={windValue} />
        )}

        {visibleComponents.includes("temperatureGraph") && (
          <TemperatureGraph
            hourlyLabels={hourlyLabels}
            temperatureSeries={temperatureSeries}
          />
        )}

        {visibleComponents.includes("averageTemperature") && (
          <AverageTemperature temperatureValue={temperatureValue} />
        )}

        {visibleComponents.includes("flaggedSensors") && (
          <FlaggedSensors hasFlaggedSensors={hasFlaggedSensors} />
        )}

        {visibleComponents.includes("humidityGraph") && (
          <HumidityGraph
            hourlyLabels={hourlyLabels}
            humiditySeries={humiditySeries}
          />
        )}

        {visibleComponents.includes("averageHumidity") && (
          <AverageHumidity humidityValue={humidityValue} />
        )}
      </div>

      <div className="mt-6">
        <div className=" bottom-10 left-10">
          <h1 className="text-7xl font-bold">Team Name</h1>
        </div>
      </div>
    </div>
  );
}
