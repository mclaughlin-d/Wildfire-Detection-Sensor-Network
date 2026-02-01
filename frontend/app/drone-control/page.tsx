import Link from "next/link";
import { FaArrowRight } from "react-icons/fa";

export default function DroneControl() {
  return (
    <div className="flex items-center justify-center min-h-screen">
      <div className="grid grid-rows-2 mt-20 mb-20 gap-4">
        <div
          className="p-12 justify-center items-center flex rounded-lg mb-4 text-2xl font-semibold"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Drone Status: Not Deployed
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
