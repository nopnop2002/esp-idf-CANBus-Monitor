#!/usr/bin/env python3
#-*- encoding: utf-8 -*-
import sys
import select, socket
import signal
import argparse

def handler(signal, frame):
	print('handler')
	sys.exit()

if __name__=='__main__':
	signal.signal(signal.SIGINT, handler)
	parser = argparse.ArgumentParser()
	parser.add_argument('--port', type=int, help='udp port', default=9876)
	args = parser.parse_args()
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	#s.bind(('<broadcast>', 9876))
	s.bind(('<broadcast>', args.port))
	s.setblocking(0)

	while True:
		result = select.select([s],[],[])
		msg = result[0][0].recv(1024)
		if (type(msg) is bytes):
			msg=msg.decode('utf-8')
		print(msg.strip())
