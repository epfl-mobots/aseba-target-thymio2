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


def main():
   args = parse_args()
   conn = serial.Serial(args.port, args.baudrate, timeout= None)

    # Then read the whole sample out
   buf = bytes()

   i = 0
   j = 0

    # while(conn.inWaiting() > 1):
    #     conn.read(1)
    # while(i < 5000):
    #     time1 = millis()
    #     time2 = millis() - time1
    #     conn.write("c".encode())
        
    #     c = conn.read(1)
    #     time3 = millis()-time1
    #     print(c,'time1 = ',time1,'time 2 = ',time2,'time3 = ',time3)
    #     i = i+1

##echo mode
   if(args.mode == "echo"):
     i = 1
     sendBuff = "C{:08}DC{:08}DC{:08}DC{:08}DC{:08}DC{:08}D\n\r".format(i,i+1,i+2,i+3,i+4,i+5)
     conn.write(sendBuff.encode())
     while(1):
       
        if(conn.inWaiting()):         
           c = conn.read(62)
           print(c,'nb = ',i)
           i = i+1
           sendBuff = "C{:08}DC{:08}DC{:08}DC{:08}DC{:08}DC{:08}D\n\r".format(i,i+1,i+2,i+3,i+4,i+5)
           #print(sendBuff)
           conn.write(sendBuff.encode())
           ##uncomment the following to add wait time
           # if((i%30000) == 0):
           #  time = millis()
           #  while((millis()-time) < 5000):
           #    j = 1
##send mode
   elif(args.mode == "read"):
     i = 1
     while(1):
       c = conn.read(62)
       # time3 = millis()-time1
       print(c,'nb = ',i)
       i = i+1
       ##uncomment the following to add wait time
       # if((i%30000) == 0):
       #    time = millis()
       #    while((millis()-time) < 5000):
       #      j = 1
           


   conn.close()


if __name__ == '__main__':
    main()
