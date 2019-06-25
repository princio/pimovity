import io
import socket
import struct
import time
import picamera

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

#imageSize = (640, 480)
#fs = cv2.FileStorage("camera_calib.xml")
#matrix = fs.getNode("camera_matrix")
#dist_coeff = fs.getNode("distortion_coefficients")
#opt_matrix = cv2.getOptimalNewCameraMatrix(matrix, dist_coeffs, imageSize, 1, imageSize, 0)
#und_map = cv2.InitUndistortRectifyMap(matrix, distCoeffs, numpy.empty([3,3]), newCameraMatrix, map1, map2)

while True:
    try:
        m = '2k'
        if   m == '4k':
            res = '3280x2464'
            s_mode = 2
            w = 3280
            h = 2464
        elif m == '2k':
            res = '1640x1232'
            s_mode = 4
            w = 1640
            h = 1232
        elif m == '1080':
            res = '1920x1080'
            s_mode = 1
            w = 1920
            h = 1080
        else:
           raise
        print(res)
        
        connection = -1
        client_socket = socket.socket()
        client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1);
        print("connecting...")
        client_socket.connect(('192.168.1.161', 8000))
        print("done!")

        b = struct.pack("<iii", 27, w, h)
        client_socket.send(b, socket.MSG_WAITALL)
        
        print("Waiting signal to start...")
        stxstx = client_socket.recv(8, socket.MSG_WAITALL)
        
        
        connection = client_socket.makefile('rwb')
        
        print("Starting...")
        output = SplitFrames(connection)
        with picamera.PiCamera(resolution=res, framerate=5) as camera:
            camera.vflip = True
            camera.hflip = True
            #camera.sensor_mode=s_mode
            #camera.resolution = res
            time.sleep(2)
            
            start = time.time()
            camera.start_recording(output, format='mjpeg')
            camera.wait_recording(3000)
            camera.stop_recording()
            print("fghjkljhg")
            # Write the terminating 0-length to the connection to let the
            # server know we're done
            #connection.write(struct.pack('<L', 0))
    except ConnectionRefusedError:
        time.sleep(1)
    finally:
        print("Closing connection...")
        if connection != -1:
            connection.close()
        client_socket.close()
        finish = time.time()
        
print('Sent %d images in %d seconds at %.2ffps' % (
    output.count, finish-start, output.count / (finish-start)))
