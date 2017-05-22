#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#  WeatherPi2.py
#  
#  Copyright 2017  <pi@weatherpi2>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#  
#  

import time
from RF24 import *
import RPi.GPIO as GPIO
import sys
import datetime
import time
import httplib, urllib
from struct import *

class TXMeasure:
	humidity = 0.0
	temperature_dht=0.0
	temperature_bmp=0.0
	pressure = 0.0
	voltage = 0.0
	voltage = 0.0
	lightlevel = 0
	dewpoint = 0.0
	charge = 0
	done = 0

PRINTSTR = "Readings: Vcc:{}mV T(dht):{}C T(bmp):{}C H:{}% P:{} L:{}% Charge:{} Done:{}"

server = "data.sparkfun.com"
public_key = "0zD0JX3rYXudLvLGQd6D"
private_key = "DngBXPadWPcZkAkGVZrz"
fields = ["time", "bmp_temp_c", "dht_temp_c",  "dewpoint", "humidity", "lightlevel", "pressure", "voltage","charge", "done", "temp_f"]
	
pipes = [0xF0F0F0F0E1, 0xF0F0F0F0D1]
radio = RF24(22, 0)

def unpackTXMeasure(buffer):
	mydata = TXMeasure
	print(len(buffer))
	#	if len(buffer) != 15:
	#		return none
	try:
		print([chr(i) for i in buffer])
		rdata = unpack("<c??lffffff", "".join(map(chr,buffer)))
		#mydata.valid = rdata[2]!=0
		print(rdata)
		mydata.charge = rdata[1]
		mydata.done = rdata[2]				
		mydata.lightlevel = rdata[3]
		mydata.humidity = rdata[4]
		mydata.temperature_dht = rdata[5]
		mydata.temperature_bmp = rdata[6]
		mydata.pressure = rdata[7]
		mydata.voltage = rdata[8]
		mydata.dewpoint = rdata[9]

		
		return mydata
	except Exception as e:
		print(e) 
		return None




def main():
	print(sys.version)
	irq_gpio_pin = None
         
	radio.begin()
	radio.enableDynamicPayloads()
	#radio.setRetries(15,15)
	radio.setChannel(70)
	#radio.setChannel(0x4c)
	#radio.setDataRate(NRF24.BR_250KBPS)
	#radio.setPALevel(NRF24.PA_MAX)
	radio.setAutoAck(0)
	radio.openWritingPipe(pipes[0])
	radio.startListening()
	radio.stopListening()
	radio.printDetails()
	
	print('Awaiting transmission')


	#radio.openWritingPipe(pipes[0])
	radio.openReadingPipe(1,pipes[1])
	radio.startListening()
    	
	
	# Pong back role.  Receive each packet, dump it out, and send it back
	while True:
		is_data_found = False

		while not is_data_found:		
			is_data_found, pipe = radio.available_pipe()
			if not is_data_found:
				time.sleep(1)
		
		size = radio.getDynamicPayloadSize()
		receive_payload = radio.read(size)
		
		if len(receive_payload) > 4:
			if receive_payload[0] == ord('*'):
				data = {}
				data[fields[0]] = time.strftime("%A %B %d, %Y %H:%M:%S %Z")
				print("data found: " + str(datetime.datetime.now()))
				#"bmp_temp", "dht_temp", "dewpoint", "humidity", "lightlevel", "pressure", "voltage"
				struct = unpackTXMeasure(receive_payload)				
				if struct != None:
					data[fields[1]] = struct.temperature_bmp 
					data[fields[2]] = struct.temperature_dht
					data[fields[3]] = struct.dewpoint
					data[fields[4]] = struct.humidity
					data[fields[5]] = struct.lightlevel
					data[fields[6]] = struct.pressure
					data[fields[7]] = struct.voltage
					data[fields[8]] = struct.charge
					data[fields[9]] = struct.done
					data[fields[10]] = struct.temperature_dht * 1.8 + 32
					params = urllib.urlencode(data)
					
					headers = {}
					headers["Content-Type"] = "application/x-www-form-urlencoded"
					headers["Connection"] = "close"
					headers["Content-Length"] = len(params) # length of data
					headers["Phant-Private-Key"] = private_key
					try:
						c = httplib.HTTPConnection(server)
						c.request("POST", "/input/" + public_key + ".txt", params, headers)
						r = c.getresponse()
						print(r.status)
						print(r.reason)						
					except:
						pass

					print(PRINTSTR.format(struct.voltage,  struct.temperature_dht, struct.temperature_bmp, struct.humidity, struct.pressure, struct.lightlevel, struct.charge, struct.done))
			else:
				print("invalid data received")

if __name__ == '__main__':
	main()

