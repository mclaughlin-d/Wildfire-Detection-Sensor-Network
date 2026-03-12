"use client";
import { useState, useRef, useEffect } from "react";
import { DASHBOARD_COMPONENTS } from "./ComponentList";

import { IoMdSettings } from "react-icons/io";

//takes list of selected components as input
interface DashboardSelectionPanelProps {
  visibleComponents: string[];
  setVisibleComponents: (components: string[]) => void;
}

export default function DashboardSelectionPanel({
  visibleComponents,
  setVisibleComponents,
}: DashboardSelectionPanelProps) {
  const [isOpen, setIsOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement>(null);

  //menu closes when you click outside it
  useEffect(() => {
    function handleClickOutside(event: MouseEvent) {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) {
        setIsOpen(false);
      }
    }
    if (isOpen) {
      document.addEventListener("mousedown", handleClickOutside);
    }
    return () => document.removeEventListener("mousedown", handleClickOutside);
  }, [isOpen]);

  const handleToggle = (componentId: string) => {
    if (visibleComponents.includes(componentId)) {
      // Remove component
      setVisibleComponents(
        visibleComponents.filter((id) => id !== componentId),
      );
    } else {
      // Add component
      setVisibleComponents([...visibleComponents, componentId]);
    }
  };

  return (
    <div ref={menuRef} className="absolute top-8 left-8 z-50">
      {/* Button to open/close */}
      <button
        onClick={() => setIsOpen(!isOpen)}
        className="text-white px-4 py-2 rounded-lg hover:opacity-80 shadow-lg"
        style={{ backgroundColor: "var(--secondary-color)" }}
      >
        <IoMdSettings className="inline-block mr-2" />
        Customize Dashboard
      </button>

      {/* Dropdown panel */}
      {isOpen && (
        <div className="absolute top-12 left-0 bg-white shadow-lg rounded-lg p-4 min-w-[250px]">
          <h3 className="font-bold mb-3 text-gray-800">Select Components:</h3>
          <div className="space-y-2">
            {Object.values(DASHBOARD_COMPONENTS).map((option) => (
              <label
                key={option.id}
                className="flex items-center gap-2 cursor-pointer hover:bg-gray-100 p-2 rounded"
              >
                <input
                  type="checkbox"
                  checked={visibleComponents.includes(option.id)}
                  onChange={() => handleToggle(option.id)}
                  className="w-4 h-4 cursor-pointer"
                />
                <span className="text-gray-700">{option.name}</span>
              </label>
            ))}
          </div>
        </div>
      )}
    </div>
  );
}
