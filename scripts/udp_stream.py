import cv2
import socket
import numpy as np
import base64
import pickle
import struct


SERVER_IP = '127.0.0.1'
SERVER_PORT = 5600

IMG_WIDTH = 4602
IMG_HEIGHT = 2592

BUFF_SIZE = IMG_WIDTH * IMG_HEIGHT

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, BUFF_SIZE) # Set a large buffer size
sock.bind((SERVER_IP, SERVER_PORT))

print(f"Listening for UDP packets on {SERVER_IP}:{SERVER_PORT}")


while True:
    # 2. Receive data packet and sender address
    packet, addr = sock.recvfrom(BUFF_SIZE)

    # if len(packet) != 1440:
    #     continue
    print(f"Received len {len(packet)}")
    # try:
    #     data = base64.b64decode(packet, ' /')
    # except:
    #     print(f"Received {packet}, retrying")
    #     print(f"Received length {len(packet)}")
    #     continue
    npdata = np.frombuffer(bytearray(packet), dtype=np.uint8)

    # 3. Decode the frame using OpenCV
    # The image data is sent as a JPEG compressed stream, which OpenCV decodes
    frame = cv2.imdecode(npdata, cv2.IMREAD_COLOR)

    # 4. Display the frame
    if frame is not None:
        cv2.imshow("RECEIVING VIDEO", frame)
        # Add frame rate display if you have a sender that provides timing info or calculate locally

    # 5. Handle exit condition
    key = cv2.waitKey(1) & 0xFF
    if key == ord('q'):
        sock.close()
        break

cv2.destroyAllWindows()