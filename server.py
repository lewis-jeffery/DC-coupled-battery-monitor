#!/usr/bin/python
import minimalmodbus
import time
import socket

fixed_output = False
fixed_value = 0.

#Setup UDP server
SERVER_IP    = ""
SERVER_PORT  = 8000
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((SERVER_IP, SERVER_PORT))
#print(f"Listening for clients")

#Setup RS485
port         = '/dev/ttySC0'
address      = 1
debug        = False
baudrate     = 9600
instrument = minimalmodbus.Instrument(port, address, debug = debug)  # port name, slave add>
instrument.serial.baudrate = baudrate                                # Baud
instrument.serial.bytesize = 8                                       # bytesize
instrument.serial.stopbits = 1                                       # stop bits
instrument.serial.timeout  = 0.5                                     # seconds
instrument.mode = minimalmodbus.MODE_RTU                             # rtu or ascii mode
instrument.clear_buffers_beforetouch_each_transaction = True


def twos_compliment_16(value):
    return -(value & 0x8000) | (value & 0x7fff)

while True:
    data, address = sock.recvfrom(4096)

    if data=='get data'.encode():
       # print(f"Received {data} from {address}")
        if not fixed_output:
            sign=1
            running_state = int(instrument.read_register(13001-1, 0, functioncode=4))
            battery_current = instrument.read_register(13021-1, 1, functioncode=4)
            battery_SoC = instrument.read_register(13023-1, 1, functioncode=4)
            load_power = twos_compliment_16(instrument.read_register(13008-1,0,functioncode=4))
            if (running_state & 0x04):
                sign=-1
            message = str(sign*battery_current)  +','+str(battery_SoC) +','+str(load_power)
        else:
            message =str(fixed_value)
        

    if data=='battery current'.encode():
       # print(f"Received {data} from {address}")
        if not fixed_output:
            sign=1
            running_state = int(instrument.read_register(13001-1, 0, functioncode=4))
            battery_current = instrument.read_register(13021-1, 1, functioncode=4)
            #battery_SoC = instrument.read_register(13023-1, 1, functioncode=4)
            #load_power = twos_compliment_16(instrument.read_register(13008-1,0,functioncode=4)>
            if (running_state & 0x04):
                sign=-1
            message = str(sign*battery_current) #  +','+str(battery_SoC) +','+str(load_power)
        else:
            message =str(fixed_value)


    if data=='battery voltage'.encode():
        battery_voltage = instrument.read_register(13020-1, 1, functioncode=4)
        message = str(battery_voltage) 

    if data=='grid frequency'.encode():
        grid_frequency = instrument.read_register(5036-1, 1, functioncode=4)
        message = str(grid_frequency) 

    if data=='battery power'.encode():
       if not fixed_output:
          sign=1
          running_state = int(instrument.read_register(13001-1, 0, functioncode=4))
          battery_power = instrument.read_register(13022-1,0,functioncode=4)
          if (running_state & 0x04):
                sign=-1
          message=str(sign*battery_power)
       else:
          message =str(fixed_value)

        

    sock.sendto(message.encode(), (address))

    if 'set'.encode() in data:
        fixed_output = True
        data_string = data.decode().split(' ')
        fixed_value = data_string[1]

    if 'clear'.encode() in data:
        fixed_output = False
