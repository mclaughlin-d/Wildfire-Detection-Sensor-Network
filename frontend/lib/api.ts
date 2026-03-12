export type ModuleSummary = {
  id: string;
  flagged: boolean;
  latitude: number;
  longitude: number;
};

export type ModuleReading = {
  _id: string;
  module_id: string;
  timestamp: string;
  temperature: number;
  humidity: number;
  gas_raw: number;
  pack_voltage: number;
};

const API_BASE_URL = process.env.NEXT_PUBLIC_API_BASE_URL ?? "http://localhost:5001";

//fetch list of modules
export async function fetchModules() {
  const response = await fetch(`${API_BASE_URL}/api/modules`, {
    cache: "no-store",
  });

  if (!response.ok) {
    throw new Error(`Failed to load modules (status ${response.status})`);
  }

  const data = (await response.json()) as { modules: ModuleSummary[] };

  return data.modules;
}

export async function fetchModuleReadings(moduleId: string, limit = 100) {
  //fetch readings for a specific module
  const response = await fetch(
    `${API_BASE_URL}/api/modules/${moduleId}/readings?limit=${limit}`,
    {
      cache: "no-store",
    },
  );

  if (!response.ok) {
    throw new Error(`Failed to load readings (status ${response.status})`);
  }

  const data = (await response.json()) as { readings: ModuleReading[] };

  return data.readings;
}