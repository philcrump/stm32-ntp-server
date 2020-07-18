#!/usr/bin/env python3

import socket
from time import time, sleep

#UDP_IP = '192.168.100.195'
UDP_IP = '10.42.0.10'
UDP_PORT = 123

class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    OFF = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
send_sock.settimeout(1.0)

#sock = socket.socket(socket.AF_INET, # Internet
#                     socket.SOCK_DGRAM) # UDP
#sock.bind((UDP_IP, UDP_PORT))

start_time = 0
az_received = 0

while True:

    MESSAGE = bytearray.fromhex('0102030405')
    #MESSAGE.extend((SPEED.to_bytes(2, byteorder='little', signed=True)))
    send_sock.sendto(MESSAGE, (UDP_IP, UDP_PORT))

    data, addr = send_sock.recvfrom(1024) # buffer size is 1024 bytes

    print("RX!", "".join("%02x" % b for b in data))
    ts = int.from_bytes(data[0:4], byteorder='little', signed=False)
    print(f"TS: %d" % (ts))

    if data[4] == 0xFF:
        # ADC Packet
        joystick_el_raw = int.from_bytes(data[5:9], byteorder='little', signed=False)
        joystick_az_raw = int.from_bytes(data[9:13], byteorder='little', signed=False)

        print(f"[Joystick] Elevation: 0x%08X, Azimuth: 0x%08X" % (joystick_el_raw, joystick_az_raw))

    #else:
    #   print(hex(sid))

    #send_sock.close()
    sleep(1)