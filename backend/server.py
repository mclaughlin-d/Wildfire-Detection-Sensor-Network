from datetime import datetime, timezone
from flask import Flask, jsonify, request
from flask_cors import CORS
from storage import get_module_by_id, get_modules, init_db, insert_reading, get_module_readings

app = Flask(__name__)

CORS(app, resources={r"/api/*": {"origins": "*"}})

#initialize db
init_db()

#check
@app.get("/api/health")
def health_check():
    return jsonify({"status": "ok", "time": datetime.now(timezone.utc).isoformat()})

#get list of all modules with basic data (id, flagged boolean, latitude, longitude)
@app.get("/api/modules")
def list_modules():
    return jsonify({"modules": get_modules()})

#get basic data for module with given id, returns 404 if not found
@app.get("/api/modules/<module_id>")
def get_module(module_id):
    module = get_module_by_id(module_id)
    if module:
        return jsonify({"module": module})
    return jsonify({"error": "not_found", "message": "Module not found"}), 404

#post a new sensor reading from a module
@app.post("/api/readings")
def post_reading():
    """
    store a reading in MongoDB
    
    Expected JSON payload:
    {
        "module_id": "mod-001",
        "sequence_num": 1,
        "row_sequence": 0,
        "timestamp": "2024-06-01T12:00:00Z",
        "gas_sensor": 123,
        "temperature": 25.0,
        "humidity": 50.0,
        "pack_voltage": 3.7,
        "payload":
    }
    """
    data = request.get_json()

    required_fields = ["module_id", "temperature", "humidity", "pack_voltage", "gas_sensor"]
    if not data or not all(field in data for field in required_fields):
        return jsonify({
            "error": "bad_request",
            "message": "Missing required reading fields"
        }), 400

    fire_confidence = data.get("fire_confidence")

    result = insert_reading(
        module_id=data["module_id"],
        sequence_num=data.get("sequence_num"),
        row_sequence=data.get("row_sequence"),
        timestamp=data.get("timestamp"),
        gas_sensor=int(data["gas_sensor"]),
        temperature=float(data["temperature"]),
        humidity=float(data["humidity"]),
        pack_voltage=float(data["pack_voltage"]),
        fire_confidence=float(fire_confidence) if fire_confidence is not None else None,
        payload=data.get("payload", [])
    )
    return jsonify({
        "status": "created",
        "inserted_id": result["inserted_id"],
        "timestamp": result["timestamp"]
    }), 201

#get recent readings for a specific module
@app.get("/api/modules/<module_id>/readings")
def get_readings(module_id):
    """
    retrieve recent readings for a given module id

    limit: max number of readings returned (default 100)
    """
    #get limit from query params
    limit = request.args.get("limit", default=100, type=int)

    readings = get_module_readings(module_id, limit=limit)
    return jsonify({
        "module_id": module_id,
        "count": len(readings),
        "readings": readings
    }), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001, debug=True)
