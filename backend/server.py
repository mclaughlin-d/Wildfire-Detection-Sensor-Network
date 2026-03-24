from datetime import datetime, timezone
from flask import Flask, jsonify, request, Response
from flask_cors import CORS
from storage import get_module_by_id, get_modules, init_db, insert_reading, get_module_readings
import gi
import threading
import time
import queue

gi.require_version('Gst', '1.0')
gi.require_version('GstApp', '1.0')
gi.require_version("GstRtspServer", "1.0")
from gi.repository import Gst, GstApp, GLib, GstRtspServer


app = Flask(__name__)

CORS(app, resources={r"/api/*": {"origins": "*"}})

#initialize db
init_db()


Gst.init(None)


frame_queue = queue.Queue(maxsize=2)  # small buffer — drop old frames
pipeline = None
glib_loop = None

def on_new_sample(appsink):
    """Called by GStreamer whenever a new frame is ready."""
    sample = appsink.emit("pull-sample")
    if sample is None:
        return Gst.FlowReturn.ERROR

    buf = sample.get_buffer()
    success, map_info = buf.map(Gst.MapFlags.READ)
    if not success:
        return Gst.FlowReturn.ERROR

    frame_bytes = bytes(map_info.data)
    buf.unmap(map_info)

    # Drop oldest frame if the queue is full (keeps latency low)
    if frame_queue.full():
        try:
            frame_queue.get_nowait()
        except queue.Empty:
            pass
    frame_queue.put(frame_bytes)

    return Gst.FlowReturn.OK

def build_pipeline(source: str = "test") -> Gst.Pipeline:
    if source == "test":
        pipeline_str = (
            "videotestsrc is-live=true pattern=ball ! "
            "videoconvert ! "
            "videoscale ! video/x-raw,width=640,height=480 ! "
            "jpegenc quality=85 ! "
            "appsink name=sink emit-signals=true sync=false max-buffers=1 drop=true"
        )
    elif source == "webcam":
        pipeline_str = ("v4l2src device=/dev/video1 ! " 
                        "videoscale ! video/x-raw,width=640,height=480,format=YUY2,framerate=30/1 ! "  "videoconvert ! "
                        "jpegenc quality=85 ! "
            "appsink name=sink name=sink emit-signals=true max-buffers=1")
        
    elif source == "drone":
        pipeline_str = (
            "udpsrc address=127.0.0.1 port=5600 ! application/x-rtp,media=(string)video,encoding-name=(string)H264 ! rtph264depay ! decodebin ! "
            "videoconvert ! "
            "jpegenc quality=85 ! "
            "appsink name=sink name=sink emit-signals=true max-buffers=1"
        )
    p = Gst.parse_launch(pipeline_str)

    sink = p.get_by_name("sink")
    sink.connect("new-sample", on_new_sample)
    return p

def start_gstreamer(source: str = "test"):
    """Start the GStreamer pipeline in a background GLib main loop."""
    global pipeline, glib_loop

    pipeline = build_pipeline(source)
    pipeline.set_state(Gst.State.PLAYING)

    glib_loop = GLib.MainLoop()
    thread = threading.Thread(target=glib_loop.run, daemon=True)
    thread.start()


def stop_gstreamer():
    global pipeline, glib_loop
    if pipeline:
        pipeline.set_state(Gst.State.NULL)
    if glib_loop:
        glib_loop.quit()


start_gstreamer(source="drone")

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

    gas_sensor = data.get("gas_sensor", data.get("gas_raw")) if data else None
    required_fields = ["module_id", "temperature", "humidity", "pack_voltage"]
    if not data or not all(field in data for field in required_fields) or gas_sensor is None:
        return jsonify({
            "error": "bad_request",
            "message": "Missing required reading fields"
        }), 400

    result = insert_reading(
        module_id=data["module_id"],
        sequence_num=data.get("sequence_num"),
        row_sequence=data.get("row_sequence"),
        timestamp=data.get("timestamp"),
        gas_sensor=int(gas_sensor),
        temperature=float(data["temperature"]),
        humidity=float(data["humidity"]),
        pack_voltage=float(data["pack_voltage"]),
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

def generate_mjpeg():
    """Generator that yields MJPEG frames for the multipart HTTP response."""
    while True:
        try:
            frame = frame_queue.get(timeout=50.0)
        except queue.Empty:
            continue

        yield (
            b"--frame\r\n"
            b"Content-Type: image/jpeg\r\n\r\n"
            + frame +
            b"\r\n"
        )

@app.post("/api/drone/stream/source")
def set_stream_source():
    """Hot-swap the GStreamer source at runtime (test / v4l2 / rtsp)."""
    data = request.get_json()
    source = data.get("source", "test") if data else "test"
    if source not in ("test", "v4l2", "rtsp"):
        return jsonify({"error": "invalid source"}), 400

    stop_gstreamer()
    time.sleep(0.5)
    start_gstreamer(source=source)
    return jsonify({"status": "ok", "source": source})

@app.get("/api/drone/stream")
def post_video():
    return Response(
        generate_mjpeg(),
        mimetype="multipart/x-mixed-replace; boundary=frame",
        headers={
            "Cache-Control": "no-cache, no-store, must-revalidate",
            "Pragma": "no-cache",
            "Expires": "0",
        }
    )

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5001, debug=True)
