#!/usr/bin/env python3
"""
Reads sounds from the F7 discovery kit microphone
"""
import argparse
# import progressbar
import serial
# import wave
import struct

from datetime import datetime
from datetime import timedelta
from threading import Thread

start_time = datetime.now()

# returns the elapsed milliseconds since the start of the program
def millis():
   dt = datetime.now() - start_time
   ms = (dt.days * 24 * 60 * 60 + dt.seconds) * 1000 + dt.microseconds / 1000.0
   return ms


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Serial port")
    parser.add_argument("mode", help="echo or read")
    parser.add_argument(
        "--length",
        "-l",
        help="Buffer length (44.1k by default)",
        type=int,
        default=44100)
    parser.add_argument(
        "-b",
        "--baudrate",
        type=int,
        default=921600,
        help="Baudrate, default=921600")

    parser.add_argument(
        "--left",
        help="Use left/U5 microphone (default is right/U6)",
        action="store_true")

    parser.add_argument(
        "--gain",
        type=float,
        help="Manual gain for the signal (default is auto)")

    return parser.parse_args()


#thread used to send
class send_thread(Thread):

    #init function called when the thread begins
    def __init__(self, port):
        Thread.__init__(self)
        self.alive = True
        self.port = port

    #function called after the init
    def run(self):
        i = 1
        while(self.alive):
            sendBuff = "C{:08}DC{:08}DC{:08}DC{:08}DC{:08}DC{:08}D\n\r".format(i,i+1,i+2,i+3,i+4,i+5)
            self.port.write(sendBuff.encode())
            i = i + 1

    #clean exit of the thread if we need to stop it
    def stop(self):
        self.alive = False
        self.join()


#thread used to receive
class receive_thread(Thread):

    #init function called when the thread begins
    def __init__(self, port):
        Thread.__init__(self)
        self.alive = True
        self.port = port

    #function called after the init
    def run(self):
        j = 1
        while(self.alive):
            if(self.port.inWaiting()):         
                c = self.port.read(62)
                print(c,'nb = ',j)
                j = j+1

    #clean exit of the thread if we need to stop it
    def stop(self):
        self.alive = False
        self.join()
        if(self.port.isOpen()):
            while(self.port.inWaiting() > 0):
                self.port.read(self.port.inWaiting())
                time.sleep(0.01)
            self.port.close()

def main():
   args = parse_args()
   conn = serial.Serial(args.port, args.baudrate, timeout= None)

    # Then read the whole sample out
   buf = bytes()

   i = 0
   j = 0

   send_thd = send_thread(conn)
   if(args.mode == "echo"):
      send_thd.start()

   rcv_thd = receive_thread(conn)
   rcv_thd.start()

   # conn.close()


if __name__ == '__main__':
    main()
