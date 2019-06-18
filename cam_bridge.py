import io
import socket
import struct
import time
import numpy as np
import cv2


class SplitFrames(object):
    def __init__(self, connection):
        self.connection = connection
        self.stream = io.BytesIO()
        self.streamin = io.BytesIO()
        self.count = 0
        self.elaborated = 0

    def write(self, buf):
        if buf.startswith(b'\xff\xd8'):
            # Start of new frame; send the old one's length
            # then the data
            size = self.stream.tell()
            print(size)
            if size > 0:
                self.connection.write(struct.pack('<L', 27692))
                self.connection.write(struct.pack('<L', size))
                self.connection.flush()
                self.stream.seek(0)
                bo = self.stream.read(size)
                self.connection.write(bo)
                self.connection.flush()
                #with open("images/img%3d.jpg" % self.count,'wb') as out: ## Open temporary file as bytes
                #    out.write(bo)                   ## Read bytes into file
                self.count += 1
                self.stream.seek(0)
                
        self.stream.write(buf)

mode = '2k'

if   mode == '2k':
    imageSize = (1640, 1232)
elif mode == '4k':
    imageSize = (3280, 2464)
elif mode == '1080':
    imageSize = (1920, 1080)
else:
    raise

fs = cv2.FileStorage("calib_%s.xml" % mode, cv2.FILE_STORAGE_READ)
matrix = fs.getNode("camera_matrix").mat()
dist_coeffs = fs.getNode("distortion_coefficients").mat()
opt_matrix, roi = cv2.getOptimalNewCameraMatrix(matrix, dist_coeffs, imageSize, 1, imageSize)
mapx, mapy = cv2.initUndistortRectifyMap(matrix, dist_coeffs, np.empty([3,3]), opt_matrix, imageSize, cv2.CV_32FC1)

RASP=True
UNITY=True

print(roi)

print("calibration ended.")

try:
    STX = b',l\x00\x00'
    ADDR = ("0.0.0.0", 8008)
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(ADDR)
    listener.listen(2)

    conn1, addr = listener.accept()
    if UNITY :
        conn2, addr = listener.accept()

    b1 = struct.unpack("<L", conn1.recv(4, socket.MSG_WAITALL))[0]
    print(b1)

    if UNITY :
        b2 = struct.unpack("<L", conn2.recv(4, socket.MSG_WAITALL))[0]
        print(b2)
        if 27 == b1 and 54 == b2:
            conn_pi = conn1
            conn_uy = conn2
        elif 54 == b1 and 27 == b2:
            conn_pi = conn2
            conn_uy = conn1
        else:
            raise
    else:
        conn_pi = conn1

    conn_pi.send(STX)

    if UNITY:
        conn_uy.send(STX)

    count = 0

    x, y, w, h = roi


    while True:
        b1 = conn_pi.recv(8, socket.MSG_WAITALL)

        if UNITY:
            conn_uy.send(b1)
            
        stx = struct.unpack("<L", b1[0:4])
        if b1[0:4] != STX: #,l\x00\x00
            print(b1[0:4])
            raise
        l = struct.unpack("<L", b1[4:8])[0]


        print("#%d: recv %d" % (count, l))

        b_jpg = conn_pi.recv(l, socket.MSG_WAITALL)

        if UNITY:
            conn_uy.send(b_jpg)

        np_jpg = np.fromstring(b_jpg, np.uint8)

        img = cv2.imdecode(np_jpg, cv2.IMREAD_COLOR)
        print(img.shape)
        dst = cv2.undistort(img, matrix, dist_coeffs, None, opt_matrix)
        #dst = cv2.remap(img, mapx, mapy, cv2.INTER_LINEAR)
        print(dst.shape)
        dst = dst[y:y+h, x:x+w]
        print(dst.shape)

        #open('images2/prova_%d.jpg' % count, 'wb').write(b_jpg)
        #cv2.imwrite('images2/prova_%d_calib.bmp' % count, dst)

        #image = np.resize(np.frombuffer(b_img, dtype=np.uint8, count=self.imageSize), (1920, 1080, 3))
        #img = cv2.imdecode("./images2/prova_%d.jpg" % count, image)

        count += 1
except OSError:
    print("[Errno 98] Address already in use")
finally:
    print("Closing connection...")
    listener.close()
    finish = time.time()
    print('Sent %d images in %d seconds at %.2ffps' % (
        output.count, finish-start, output.count / (finish-start)))
