all: list2 letterman

list2:
	g++ -std=c++0x device.cc mounted_devices.cc list2.cc -g -o list2 -lhivex -Wall
letterman:
	g++ -std=c++0x device.cc mounted_devices.cc letterman.cc -g -o letterman -lhivex -Wall

