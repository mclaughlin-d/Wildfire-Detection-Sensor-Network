import os
from datetime import datetime, timezone
from bson import ObjectId
from pymongo import MongoClient
from dotenv import load_dotenv

#load environment variables
load_dotenv()

_client = None
_db = None

def init_db():
    """Initialize MongoDB connection and create indexes"""
    global _client, _db
    uri = os.getenv("MONGODB_URI")
    db_name = os.getenv("MONGODB_DB", "capstone")
    
    _client = MongoClient(uri, serverSelectionTimeoutMS=5000)
    _db = _client[db_name]
    
    #create indexes
    readings = _db["readings"]
    readings.create_index([("module_id", 1), ("timestamp", -1)])
    
    print(f"connected to {db_name} on MongoDB Atlas")

def get_db():
    """Get database instance, initialize if needed"""
    global _db
    if _db is None:
        init_db()
    return _db

#private module list stored in backend
_modules = [
    {
        "id": "mod-001",
        "flagged": True,
        "latitude": 42.3601,
        "longitude": -71.0589,
    },
    {
        "id": "mod-002",
        "flagged": False,
        "latitude": 42.3467,
        "longitude": -71.0972,
    },
    {
        "id": "mod-003",
        "flagged": False,
        "latitude": 42.3398,
        "longitude": -71.0892,
    },
    {
        "id": "mod-004",
        "flagged": False,
        "latitude": 42.3519,
        "longitude": -71.0646,
    },
    {
        "id": "mod-005",
        "flagged": True,
        "latitude": 42.3655,
        "longitude": -71.0546,
    },
    {
        "id": "mod-006",
        "flagged": False,
        "latitude": 42.3432,
        "longitude": -71.0761,
    },
    {
        "id": "mod-007",
        "flagged": False,
        "latitude": 42.3321,
        "longitude": -71.1002,
    },
    {
        "id": "mod-008",
        "flagged": False,
        "latitude": 42.3321,
        "longitude": -71.1002,
    },
    {
        "id": "mod-009",
        "flagged": False,
        "latitude": 42.3321,
        "longitude": -71.1002,
    },
]

#returns list of all modules with basic data
def get_modules():
    return list(_modules)

#returns module and its data for given id, returns None if not found
def get_module_by_id(module_id):
    for module in _modules:
        if module["id"] == module_id:
            return module
    return None


#--------------------------------------------------------------------------------
#MongoDB read/write functions
def insert_reading(module_id, temperature, humidity, gas_raw, pack_voltage):
    """
    Insert a sensor reading into MongoDB.
    
    Args:
        module_id: ID of the sensor module
        temperature: temp in C? (float)
        humidity: humidity percentage (float)
        gas_raw: raw gas sensor value (0-4096)
        pack_voltage: battery pack voltage (float)
    
    Returns:
        dict with inserted_id and timestamp
    """
    db = get_db()
    readings = db["readings"]
    
    reading = {
        "module_id": module_id,
        "timestamp": datetime.now(timezone.utc),
        "temperature": temperature,
        "humidity": humidity,
        "gas_raw": gas_raw,
        "pack_voltage": pack_voltage,
    }
    
    result = readings.insert_one(reading)
    return {
        "inserted_id": str(result.inserted_id),
        "timestamp": reading["timestamp"].isoformat(),
    }

def get_module_readings(module_id, limit=100):
    """
    Retrieve recent readings for a module from MongoDB.
    
    Args:
        module_id: module ID
        limit: max number of readings you want
    
    Returns:
        List of readings sorted by timestamp (newest first)
    """
    db = get_db()
    readings = db["readings"]
    
    readings = list(
        readings.find({"module_id": module_id})
        .sort("timestamp", -1)
        .limit(limit)
    )
    
    # Convert ObjectId to string for JSON serialization
    for reading in readings:
        reading["_id"] = str(reading["_id"])
        reading["timestamp"] = reading["timestamp"].isoformat()
    
    return readings

#--------------------------------------------------------------------------------