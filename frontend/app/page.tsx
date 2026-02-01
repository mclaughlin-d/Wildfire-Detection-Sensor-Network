"use client";

import { useEffect, useState } from "react";

export default function Home() {
  const [time, setTime] = useState<string>("");
  const [date, setDate] = useState<string>("");

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

    // Update every minute
    const interval = setInterval(updateDateTime, 60000);

    return () => clearInterval(interval);
  }, []);

  return (
    <div className="ml-30 mr-10 h-screen flex flex-col p-4 pb-28">
      <div className="text-right mb-4 mt-4">
        <h1 className="text-2xl">{time}</h1>
        <h1 className="text-2xl">{date}</h1>
      </div>

      <div className="grid grid-cols-3 grid-rows-2 gap-4 flex-1">
        <div
          className="p-4 w-full h-full justify-center items-center flex rounded-lg font-semibold"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Wind Direction
        </div>
        <div
          className="p-4 w-full h-full justify-center items-center flex rounded-lg font-semibold"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Temperature Graph
        </div>
        <div
          className="p-4 w-full h-full justify-center items-center flex rounded-lg font-semibold"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Average Temperature
        </div>
        <div
          className="p-4 w-full h-full justify-center items-center flex rounded-lg font-semibold"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Flagged Sensors
        </div>
        <div
          className="p-4 w-full h-full justify-center items-center flex rounded-lg font-semibold"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Humidity Graph
        </div>
        <div
          className="p-4 w-full h-full justify-center items-center flex rounded-lg font-semibold"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Average Humidity
        </div>
      </div>

      <div className="mt-6">
        <div className="absolute bottom-10 left-10">
          <h1 className="text-7xl font-bold">Team Name</h1>
        </div>
      </div>
    </div>
  );
}
