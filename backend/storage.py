import os
from datetime import datetime, timezone
from bson import ObjectId
from pymongo import MongoClient
from dotenv import load_dotenv

#load environment variables
load_dotenv()

_client = None
_db = None
FIRE_CONFIDENCE_FLAG_THRESHOLD = 3.0

def init_db():
    """Initialize MongoDB connection and create indexes"""
    global _client, _db
    uri = os.getenv("MONGODB_URI")
    str_mongo = os.getenv("MONGO_STR")
    db_name = os.getenv("MONGODB_DB", "capstone")
    
    _client = MongoClient(str_mongo, serverSelectionTimeoutMS=5000)
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
        "flagged": False,
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
        "flagged": False,
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
    # {
    #     "id": "mod-009",
    #     "flagged": False,
    #     "latitude": 42.3321,
    #     "longitude": -71.1002,
    # },
]

def restore_flags_from_db():
    """On startup, set each module's flagged state from its latest fire_confidence reading."""
    db = get_db()
    readings_col = db["readings"]
    for module in _modules:
        latest = readings_col.find_one(
            {"module_id": module["id"], "fire_confidence": {"$ne": None}},
            sort=[("timestamp", -1)]
        )
        if latest and latest.get("fire_confidence") is not None:
            module["flagged"] = latest["fire_confidence"] > FIRE_CONFIDENCE_FLAG_THRESHOLD

#returns list of all modules with basic data
def get_modules():
    return list(_modules)

#returns module and its data for given id, returns None if not found
def get_module_by_id(module_id):
    for module in _modules:
        if module["id"] == module_id:
            return module
    return None


def _update_module_flag(module_id, fire_confidence):
    """Set module flagged status from latest fire confidence when provided."""
    if fire_confidence is None:
        return

    module = get_module_by_id(module_id)
    if module is None:
        return

    module["flagged"] = fire_confidence > FIRE_CONFIDENCE_FLAG_THRESHOLD


#--------------------------------------------------------------------------------
#MongoDB read/write functions
def insert_reading(module_id, sequence_num=None, timestamp=None, row_sequence=None,
                   temperature=None, humidity=None, gas_sensor=None,
                   pack_voltage=None, fire_confidence=None, payload=None):
    db = get_db()
    readings = db["readings"]

    _update_module_flag(module_id, fire_confidence)

    reading = {
        "module_id":   module_id,
        "sequence_num": sequence_num,
        "row_sequence": row_sequence,
        "timestamp":   datetime.now(timezone.utc),
        "gas_sensor":  gas_sensor,
        "temperature": temperature,
        "humidity":    humidity,
        "pack_voltage": pack_voltage,
        "fire_confidence": fire_confidence,
        "payload": payload

    }

    result = readings.insert_one(reading) #built in method
    return {
        "inserted_id": str(result.inserted_id),
        "timestamp":   reading["timestamp"].isoformat(),
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