import AverageHumidity from "./AverageHumidity";
import AverageTemperature from "./AverageTemperature";
import FlaggedSensors from "./FlaggedSensors";
import HumidityGraph from "./HumidityGraph";
import TemperatureGraph from "./TemperatureGraph";
import WindDirection from "./WindDirection";

export const DASHBOARD_COMPONENTS = {
  windDirection: {
    id: "windDirection",
    name: "Wind Direction",
    component: WindDirection,
  },
  temperatureGraph: {
    id: "temperatureGraph",
    name: "Temperature Progression",
    component: TemperatureGraph,
  },
  averageTemperature: {
    id: "averageTemperature",
    name: "Average Temperature",
    component: AverageTemperature,
  },
  flaggedSensors: {
    id: "flaggedSensors",
    name: "Flagged Sensors",
    component: FlaggedSensors,
  },
  humidityGraph: {
    id: "humidityGraph",
    name: "Humidity Progression",
    component: HumidityGraph,
  },
  averageHumidity: {
    id: "averageHumidity",
    name: "Average Humidity",
    component: AverageHumidity,
  },
} as const;
