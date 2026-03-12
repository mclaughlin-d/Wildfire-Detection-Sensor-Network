import Link from "next/link";
import { FaCircleArrowRight } from "react-icons/fa6";

interface FlaggedSensorsProps {
  hasFlaggedSensors: boolean;
}

export default function FlaggedSensors({
  hasFlaggedSensors,
}: FlaggedSensorsProps) {
  return (
    <div
      className="p-4 w-full h-full flex items-center justify-center rounded-lg font-semibold"
      style={{
        backgroundColor: hasFlaggedSensors ? "#d63b3b" : "#2f9e44",
      }}
    >
      {hasFlaggedSensors ? (
        <Link
          href="/map-view"
          className="flex items-center gap-4 rounded-full bg-[#f7a8a6] px-8 py-5 text-[#8a1a1a] font-semibold shadow-sm hover:bg-[#f49b98] transition"
        >
          <span className="text-lg">Flagged Sensors Detected</span>
          <FaCircleArrowRight className="text-[#8a1a1a] text-3xl" />
        </Link>
      ) : (
        <div className="flex items-center gap-4 rounded-full bg-[#9be7a2] px-8 py-5 text-[#145a22] font-semibold shadow-sm">
          <span className="text-lg">No Flagged Sensors</span>
        </div>
      )}
    </div>
  );
}
