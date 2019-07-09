import io
import socket
import struct
import time
import numpy as np

mode = '2k'

if   mode == '2k':
    imageSize = (1640, 1232)
elif mode == '4k':
    imageSize = (3280, 2464)
elif mode == '1080':
    imageSize = (1920, 1080)
else:
    raise

try:
    
    in_file = open("image.jpg", "rb")
    data = in_file.read()
    in_file.close()
    
    STX = struct.pack("<L", 27692)
    STX2 = struct.pack("<LL", 27692, 27692)
    
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    client.connect(("192.168.1.161", 8000))
    print("connected")
    
    cfg = struct.pack("<LLL", 27, 1640, 1232)
    
    client.send(cfg, 0)
    
    stxsstxs = bytes(8) #struct.pack("<LL", 27692, 27692)
    
    while stxsstxs != STX2:
        print(stxsstxs)
        stxsstxs = client.recv(8, socket.MSG_WAITALL)
        
    print("recevied start signal")
    
    header = struct.pack("<LL", 27692, len(data))
    
    count = 0
    
    while True:
        
        print(header)
        
        client.send(header, 0)
        
        client.send(data, 0)

        print("#%d" % (count))
        
        client.recv(8)
        
        time.sleep(0.1)

        count += 1
except OSError:
    print("[Errno 98] Address already in use")
finally:
    print("Closing connection...")
    finish = time.time()
    print('Sent %d images in %d seconds at %.2ffps' % (
        output.count, finish-start, output.count / (finish-start)))
