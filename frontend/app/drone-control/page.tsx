"use client";
import Link from "next/link";
import { useState, useRef } from "react";
import { FaArrowRight } from "react-icons/fa";

const STREAM_URL = "http://localhost:5001/api/drone/stream";
export default function DroneControl() {
  const [droneStatus, setDroneStatus] = useState<boolean>(false);
  const localVideoRef = useRef(null);
  const [socket, setSocket] = useState(null);

  return (
    <div className="flex items-center justify-center min-h-screen">
      <div className="grid grid-rows-3 mt-20 mb-20 gap-4">
        <div
          className="p-12 justify-center items-center flex rounded-lg mb-4 text-2xl font-semibold"
          style={{
            backgroundColor: droneStatus ? "#d63b3b" : "#2f9e44",
          }}
        >
          Drone Status: {droneStatus ? "Deployed" : "Not Deployed"}
        </div>
        <div className="flex flex-col w-full justify-center align-middle bg-gray-600 rounded-lg p-4">
          <div className="justify-center items-center flex">
            <h3 className="font-bold text-2xl">Drone Feed</h3>
          </div>
          <img
            src={STREAM_URL}
            alt="Drone feed"
            style={{ width: "640px", height: "480px", display: "block" }}
          />
        </div>
        <div
          className="p-12 justify-center flex items-center rounded-lg mt-4 text-2xl font-semibold hover:opacity-80 transition cursor-pointer"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          <Link
            href="/drone-control/past-deployments"
            className="flex items-center gap-4"
          >
            View Past Deployments <FaArrowRight />
          </Link>
        </div>
      </div>
    </div>
  );
}
