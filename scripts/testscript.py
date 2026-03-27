import cv2

print(cv2.getBuildInformation())

pipeline_str = "gst-launch-1.0 videotestsrc ! videoconvert ! autovideosink"

cap = cv2.VideoCapture(pipeline_str, cv2.CAP_GSTREAMER)

if not cap.isOpened():
    print("FLSKDJF")
    exit()

while True:
    ret, frame = cap.read()
    if not ret:
        print("Error")
        break

    cv2.imshow('sldkf', frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
