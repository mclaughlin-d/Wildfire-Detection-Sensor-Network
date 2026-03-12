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
        "temperature": x,
        "humidity": x,
        "gas_raw": x,
        "pack_voltage": x
    }
    """
    data = request.get_json()
    
    #validate fields
    required_fields = ["module_id", "temperature", "humidity", "gas_raw", "pack_voltage"]
    if not data or not all(field in data for field in required_fields):
        return jsonify({
            "error": "bad_request",
            "message": f"Missing required fields: {required_fields}"
        }), 400

    result = insert_reading(
        module_id=data["module_id"],
        temperature=float(data["temperature"]),
        humidity=float(data["humidity"]),
        gas_raw=int(data["gas_raw"]),
        pack_voltage=float(data["pack_voltage"]),
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
