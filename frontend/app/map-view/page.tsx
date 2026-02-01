export default function MapView() {
  return (
    <div className="ml-30 mr-4 mt-4 mb-4 h-screen flex flex-col p-4">
      <div className="grid grid-cols-3 grid-rows-3 gap-4 flex-1 ">
        <div
          className="w-full h-full justify-center items-center flex rounded-lg font-semibold row-span-2"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Flagged Sensors
        </div>
        <div
          className="w-full h-full justify-center items-center flex rounded-lg font-semibold col-span-2 row-span-2"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Map
        </div>
        <div
          className="w-full h-full justify-center items-center flex rounded-lg font-semibold col-span-3"
          style={{ backgroundColor: "var(--secondary-color)" }}
        >
          Selected Sensor Readings
        </div>
      </div>
    </div>
  );
}
