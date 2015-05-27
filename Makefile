CC = g++
LIBS    = ./x264/libx264.a -lpthread -ldl -lrt -lm 
IDIR    =./x264
CFLAGS=-I$(IDIR)

main:file

file:x264encoder.cpp
	$(CC) -o H264enc $? $(CFLAGS)  $(LIBS)

cam:x264encoder.cpp
	$(CC) -o H264enc $? $(CFLAGS) -DFROM_CAM -I./tools/Libcam  ./tools/Libcam/libcam.a $(LIBS)

clean:
	rm  H264enc
