import numpy as np
import torch
import torch.nn as nn
import torchvision
from torchvision import transforms
from torchvision.models import resnet50, ResNet50_Weights
from PIL import Image
import gi
gi.require_version('Gst', '1.0')
gi.require_version('GstApp', '1.0')
from gi.repository import Gst, GstApp, GLib

PRETRAINED_FIRE_PATH = '/home/dani/fire_cnn/fire_classify_trained.pth'

# Device setup
if torch.cuda.is_available():
    device = torch.device("cuda")
    print(f'There are {torch.cuda.device_count()} GPU(s) available.')
    print('Device name:', torch.cuda.get_device_name(0))
else:
    print('No GPU available, using the CPU instead.')
    device = torch.device("cpu")

# Transform pipeline
transform = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

# Load model
pretrained_state_dict = torch.load(PRETRAINED_FIRE_PATH)
model = resnet50()
model.fc = nn.Linear(model.fc.in_features, 3)  # Replace fc FIRST
result = model.load_state_dict(pretrained_state_dict)    # Then load weights
print(f"Missing keys: {result.missing_keys}")
print(f"Unexpected keys: {result.unexpected_keys}")

model = model.to(device)
model.eval()


pil_img = Image.open("nofire.png").convert("RGB")
img_tensor = transform(pil_img).unsqueeze(0).to(device)
with torch.no_grad():
    output = model(img_tensor)
    probs = torch.softmax(output, dim=1)
    print(f"Raw logits: {output}")
    print(f"Probabilities: {probs}")  # Should not be [~0, ~0, ~1] for every image
    print(f"Prediction: {torch.argmax(output, dim=1).item()}")

pil_img = Image.open("fire.png").convert("RGB")
img_tensor = transform(pil_img).unsqueeze(0).to(device)
with torch.no_grad():
    output = model(img_tensor)
    probs = torch.softmax(output, dim=1)
    print(f"Raw logits: {output}")
    print(f"Probabilities: {probs}")  # Should not be [~0, ~0, ~1] for every image
    print(f"Prediction: {torch.argmax(output, dim=1).item()}")


# NOTE: 
# likely that 0 is Smoke, 1 is fire, 2 is no fire - so predictiosn correct!!!!
    
    
# Init GStreamer
Gst.init(None)

pipeline_str = (
    "udpsrc address=127.0.0.1 port=5600 ! "
    "application/x-rtp,media=(string)video,encoding-name=(string)H264 ! "
    "rtph264depay ! decodebin ! "
    "videoconvert ! video/x-raw,format=RGB ! "
    "appsink name=sink emit-signals=true max-buffers=1 drop=true"
)

# test_pipeline_str = (
#     "multifilesrc location=smallernofire.jpg loop=true ! jpegdec ! videoconvert ! videorate ! video/x-raw,framerate=30/1 ! "
#     "appsink name=sink emit-signals=true max-buffers=1 drop=true"
# )

FILE_NAME = "nofire.png" # 717, 481
FIRE_NAME = "fire.png" # 1236, 815

# gst-launch-1.0 multifilesrc location=nofire.png caps="image/png,framerate=30/1" loop=true ! pngdec ! videoconvert ! autovideosink

test_pipeline_str = (
    f"multifilesrc location={FIRE_NAME} loop=true ! "
    "image/png,framerate=30/1,width=1236,height=815 ! "
    "pngdec ! "
    "videoconvert ! video/x-raw,format=RGB ! "
    "appsink name=sink emit-signals=true max-buffers=1 drop=true"
)

pipeline = Gst.parse_launch(test_pipeline_str)
appsink = pipeline.get_by_name("sink")

appsink.set_property("emit-signals", True)
appsink.set_property("sync", False)

def on_new_sample(sink):
    """Callback triggered for each new frame from GStreamer."""
    sample = sink.emit("pull-sample")
    if sample is None:
        return Gst.FlowReturn.ERROR

    buf = sample.get_buffer()
    caps = sample.get_caps()

    # Extract frame dimensions from caps
    structure = caps.get_structure(0)
    width = structure.get_value("width")
    height = structure.get_value("height")

    # Map buffer to numpy array
    success, map_info = buf.map(Gst.MapFlags.READ)
    if not success:
        return Gst.FlowReturn.ERROR

    try:
        expected = height * width * 3
        actual = len(map_info.data)

        if actual != expected:
            # RGB stride may be padded — compute rows from actual size
            stride = actual // height
            frame = np.frombuffer(map_info.data, dtype=np.uint8).reshape((height, stride))
            frame = frame[:, :width*3].reshape((height, width, 3))
        else:
            frame = np.frombuffer(map_info.data, dtype=np.uint8).reshape((height, width, 3))

        # frame = frame[:, :, ::-1].copy()  # BGR -> RGB flip
        # pil_img = Image.fromarray(frame)
        # frame = np.frombuffer(map_info.data, dtype=np.uint8).reshape((height, width, 3))

        # Convert to PIL and apply transforms
        pil_img = Image.fromarray(frame)

        # always predicts 2 for the nofire image
        # predicts 1 for the fire image when not doing the bgr flip
        img_tensor = transform(pil_img).unsqueeze(0).to(device)

        # Run model inference (uncomment when model is loaded)
        with torch.no_grad():
            output = model(img_tensor)
            prediction = torch.argmax(output, dim=1).item()
            print(f"Prediction: {prediction}")

    finally:
        buf.unmap(map_info)

    return Gst.FlowReturn.OK

def on_bus_message(bus, message):
    t = message.type
    if t == Gst.MessageType.ERROR:
        err, debug = message.parse_error()
        print(f"ERROR: {err}, debug: {debug}", flush=True)
        loop.quit()
    elif t == Gst.MessageType.WARNING:
        err, debug = message.parse_warning()
        print(f"WARNING: {err}, debug: {debug}", flush=True)
    elif t == Gst.MessageType.STATE_CHANGED:
        old, new, pending = message.parse_state_changed()
        print(f"State changed: {old.value_nick} -> {new.value_nick}", flush=True)
    elif t == Gst.MessageType.EOS:
        print("EOS reached", flush=True)
        loop.quit()


bus = pipeline.get_bus()
bus.add_signal_watch()
bus.connect("message", on_bus_message)

# Connect callback and start pipeline
appsink.connect("new-sample", on_new_sample)
pipeline.set_state(Gst.State.PLAYING)

# Run GLib main loop to keep callbacks firing
loop = GLib.MainLoop()
try:
    print("Streaming... Press Ctrl+C to stop.")
    loop.run()
except KeyboardInterrupt:
    print("Stopping...")
finally:
    pipeline.set_state(Gst.State.NULL)