import numpy as np
import torch
import torch.nn as nn
import argparse
from torchvision import transforms
from torchvision.models import resnet50
from PIL import Image

import gi
gi.require_version('Gst', '1.0')
gi.require_version('GstApp', '1.0')
from gi.repository import Gst, GstApp, GLib

PRETRAINED_FIRE_PATH = './fire_classify_trained.pth'

PRED_TO_LABEL = {
    0: 'Smoke',
    1: 'Fire',
    2: 'No Fire',
}

NUM_CLASSES = 3


if torch.cuda.is_available():
    device = torch.device("cuda")
    print(f'There are {torch.cuda.device_count()} GPU(s) available.')
    print('Device name:', torch.cuda.get_device_name(0))
else:
    print('No GPU available, using the CPU instead.')
    device = torch.device("cpu")

# Transform pipeline - must match the one the model was trained on!
transform = transforms.Compose([
    transforms.Resize((224, 224)),
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])


pretrained_state_dict = torch.load(PRETRAINED_FIRE_PATH)
model = resnet50()
model.fc = nn.Linear(model.fc.in_features, NUM_CLASSES)
result = model.load_state_dict(pretrained_state_dict)

model = model.to(device)
model.eval()

def on_new_sample(sink):
    """Callback triggered for each new frame from GStreamer."""
    sample = sink.emit("pull-sample")
    if sample is None:
        return Gst.FlowReturn.ERROR

    buf = sample.get_buffer()
    caps = sample.get_caps()

    structure = caps.get_structure(0)
    width = structure.get_value("width")
    height = structure.get_value("height")

    success, map_info = buf.map(Gst.MapFlags.READ)
    if not success:
        return Gst.FlowReturn.ERROR

    try:
        expected = height * width * 3
        actual = len(map_info.data)

        if actual != expected:
            stride = actual // height
            frame = np.frombuffer(map_info.data, dtype=np.uint8).reshape((height, stride))
            frame = frame[:, :width*3].reshape((height, width, 3))
        else:
            frame = np.frombuffer(map_info.data, dtype=np.uint8).reshape((height, width, 3))

        pil_img = Image.fromarray(frame)
        img_tensor = transform(pil_img).unsqueeze(0).to(device)

        with torch.no_grad():
            output = model(img_tensor)
            prediction = torch.argmax(output, dim=1).item()
            print(f"\tPrediction: {PRED_TO_LABEL[prediction]}")

    finally:
        buf.unmap(map_info)

    return Gst.FlowReturn.OK


def main():
    parser = argparse.ArgumentParser(description="A script to stream frames from a GStreamer pipeline and feed them to a pre-trained CNN")
    parser.add_argument("stream_type", help="GStreamer stream type. Either 'udp': a UDP socket (drone streaming) or 'file': a .png file (testing)")
    parser.add_argument("--file", help="Path to .png file, if using file type", default="sample_images/nofire.png")

    args = parser.parse_args()

    stream_type = args.stream_type

    pipeline_str = ""
    if stream_type == "udp":
        pipeline_str = (
            "udpsrc address=127.0.0.1 port=5600 ! "
            "application/x-rtp,media=(string)video,encoding-name=(string)H264 ! "
            "rtph264depay ! decodebin ! "
            "videoconvert ! video/x-raw,format=RGB ! "
            "appsink name=sink emit-signals=true max-buffers=1 drop=true"
        )

    elif stream_type == "file":
        try:
            img = Image.open(args.file)
        except:
            print("Invalid image path!")
            exit(1)
        
        width, height = img.size
        pipeline_str = (
            f"multifilesrc location={args.file} loop=true ! "
            f"image/png,framerate=30/1,width={width},height={height} ! "
            "pngdec ! "
            "videoconvert ! video/x-raw,format=RGB ! "
            "appsink name=sink emit-signals=true max-buffers=1 drop=true"
        )

    else:
        exit(1)


    print("Initializing GStreamer...")
    Gst.init(None)

    pipeline = Gst.parse_launch(pipeline_str)

    appsink = pipeline.get_by_name("sink")
    appsink.set_property("emit-signals", True)
    appsink.set_property("sync", False)

    appsink.connect("new-sample", on_new_sample)
    pipeline.set_state(Gst.State.PLAYING)

    loop = GLib.MainLoop()
    try:
        print("Streaming... Press Ctrl+C to stop.")
        loop.run()
    except KeyboardInterrupt:
        print("Stopping...")
    finally:
        pipeline.set_state(Gst.State.NULL)


if __name__ == "__main__":
    main()