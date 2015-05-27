CC = g++
LIBS    = -lavcodec -lswresample -lavformat -lavutil -lrt -lGL -lglut -lGLEW  -lpthread
LIBS_DIR    = ./ffmpeg/build/lib
IDIR    = ./ffmpeg/build/include
CFLAGS=-g -ggdb3 -I$(IDIR) -L$(LIBS_DIR)


all:YUV420p_GL.o Player

Player: Player.cpp 
	$(CC) -o $@ $? $(CFLAGS) YUV420p_GL.o $(LIBS) 

YUV420p_GL.o: YUV420p_GL.cpp
	$(CC)  $(CFLAGS) -c YUV420p_GL.cpp $(LIBS)

clean:
	rm *o Player
