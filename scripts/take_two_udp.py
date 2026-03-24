import cv2

import gi 
gi.require_version('Gst', '1.0')

stream_url = "udp://@127.0.0.1:5600"

while True:
    cap = cv2.VideoCapture(stream_url)

    if not cap.isOpened():
        print("Could not open!")
        break

    while True:
        ret, frame = cap.read()

        if not ret:
            print("Error receiving frame")
            break

        cv2.imshow('OpenHD stream', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    cap.release()
    cv2.destroyAllWindows()
