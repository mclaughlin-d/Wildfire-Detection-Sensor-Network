"use client";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { RiHome9Fill } from "react-icons/ri";
import { FaMapMarkerAlt } from "react-icons/fa";
import { TbDrone } from "react-icons/tb";

export default function Navbar() {
  const pathname = usePathname();

  return (
    <nav className="fixed left-0 ml-6 h-screen w-20  flex flex-col items-center justify-center gap-6">
      {/* map view */}
      <Link
        href="/map-view"
        className={`p-4 rounded-full hover:opacity-80 transition ${
          pathname === "/map-view" ? "border-2 border-white" : ""
        }`}
        style={{ backgroundColor: "var(--secondary-color)" }}
      >
        <FaMapMarkerAlt className="w-8 h-8 text-white" />
      </Link>

      {/* home dashboard view*/}
      <Link
        href="/"
        className={`p-4 rounded-full hover:opacity-80 transition ${
          pathname === "/" ? "border-2 border-white" : ""
        }`}
        style={{ backgroundColor: "var(--secondary-color)" }}
      >
        <RiHome9Fill className="w-8 h-8 text-white" />
      </Link>

      {/*drone control view */}
      <Link
        href="/drone-control"
        className={`p-4 rounded-full hover:opacity-80 transition ${
          pathname === "/drone-control" ? "border-2 border-white" : ""
        }`}
        style={{ backgroundColor: "var(--secondary-color)" }}
      >
        <TbDrone className="w-8 h-8 text-white" />
      </Link>
    </nav>
  );
}
