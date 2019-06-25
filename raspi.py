import io
import socket
import struct
import time
import picamera

class SplitFrames(object):
    def __init__(self, pi_conn, unity_conn):
        self.pi_conn = pi_conn
        self.unity_conn = unity_conn
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
                self.pi_conn.write(struct.pack('<L', 27692))
                self.unity_conn.write(struct.pack('<L', size))
                self.pi_conn.flush()
                self.unity_conn.flush()

                self.stream.seek(0)
                bo = self.stream.read(size)
                self.pi_conn.write(bo)
                self.unity_conn.write(bo)
                self.pi_conn.flush()
                self.unity_conn.flush()

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
        

        STX = 27692
        packet_config = struct.pack("<L", STX)
        packet_config += struct.pack("<L", w)
        packet_config += struct.pack("<L", h)
        # client_socket.send(packet_config, socket.MSG_WAITALL)

        pi = None
        unity = None

        connection = -1
        server_socket = socket.socket()
        server_socket.bind(('0.0.0.0', 8000))

        for i=range(1,2):
            client = server_socket.listen()

            client.send(packet_config, socket.MSG_WAITALL)

            id = struct.unpack("<L", client.recv(4))

            if id == 27:
                pi = client
                print("Connected to Pi.")
            elif id == 54:
                unity = client
                print("Connected to Unity.")
            

        # print("connecting...")
        # server_socket.accept()
        # client_socket.connect(('192.168.1.161', 8000))
        # print("done!")

        # b = struct.pack("<L", 27692)
        # b += struct.pack("<L", w)
        # b += struct.pack("<L", h)
        # client_socket.send(b, socket.MSG_WAITALL)
        
        # STX = client_socket.recv(4, socket.MSG_WAITALL)
        # print(STX)
        # print(struct.unpack("<L", STX))
        
        
        pi_conn = pi.makefile('rwb')
        unity_conn = unity.makefile('rwb')

        print("beginning...")
        output = SplitFrames(pi_conn, unity_conn)
        with picamera.PiCamera(resolution=res, framerate=10) as camera:
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
