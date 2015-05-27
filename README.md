Realtime "pseudo-scalable" video coding
=======================================
This project provides a simple and convenient framework to produce H.264/AVC "pseudo-scalable" streams. 

### Principle
A slightly modified version of x264 is used through the API to encode a raw video stream by repeating twice each input frame. The first time, the frame is encoded at low bitrate. This low quality version of the frame is inserted in the reference frame buffer and can be used at the next iteration to predict the same content at higher quality level.

This approach does not require any additional syntax elements and the stream remains AVC compliant. The only trick to deal with is the duplication of each input frame. By reusing the frame encoded at low bitrate in the prediction loop, the compression efficieny is improved compared to the classical simulcast approach

A typical application which can benefits from technique is videoconferencing with more than two simultaneous users. In videoconferencing systems, the MCU (Multipoint Control Unit) forwards a stream coming from one user to the others. With the scalable approach, the users provide their streams with different bitrate, allowing the MCU to adapt the retransmission rates with respect to the different users' bandwidth. 

### Basic setup
- **Clone directory:**

	\> git clone git clone https://github.com/tyze/scala264 

- **x264:**

	\> cd scala264   
	\> git clone git://git.videolan.org/x264.git

	Optional (but preferred): install yasm (http://yasm.tortall.net/Download.html)

	\> cd x264  
	\> git checkout 121396c .  
	\> patch -p1 -i ../x264_r2538.diff   
	\> ./configure --enable-static && make

- **Build the encoder:**

	\> cd ..   
	\> make

- **Scalable encoding from file:**

	Generate a scalable stream  (10 seconds) from  YUV raw file (pattern):

	\> ./H264enc pattern_640x480_15fps.yuv test_scala.264  10

- **Decode the stream:**

	The stream (test_scala.264) is easily decoded with a classical H.264/AVC decoder, e.g:

	\> ffmpeg -i test_scala.264 test_640x480_15fps.yuv

	test_640x480_15fps.yuv contains twice each input frame (low and high quality) and has an effective frame rate of 30fps.

### Advanced setup

- **OpenGL support (libGL, libGlew, libGLU):**

	For Ubuntu, this should do the job: 

	\> sudo apt-get install libglew-dev libglu1-mesa-dev

- **FFmpeg**

	\> cd tools/Player  
	\> git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg  
	\> cd ffmpeg  
	\> mkdir build   
	\> ./configure --prefix=./build --disable-shared --enable-static --disable-lzma --disable-bzlib --disable-zlib && make && make install

- **Build player**

	\> cd .. && make

- **Decode and play a scalable stream:**

	\> ./Player ../../test_scala.264

	It is possible to choose the quality level, interactively.

- **Encode from file and decode/play in a loop**

	\> ../../H264enc ../../pattern_640x480_15fps.yuv /dev/stdout 0  |  ./Player /dev/stdin

	Wait a few seconds before the decoded video is displayed.

- **Encode from webcam and decode/play in a loop**

	After installing v4l2 developpment environment (Ubuntu: sudo apt-get install libv4l-dev):

	\> cd ../Libcam && make  
	\> cd ../../ && make clean && make cam  
	\> ./H264enc /dev/video0 /dev/stdout  0 |  ./tools/Player/Player /dev/stdin


  

  

--------------------------------------
**Contact:** nicolas.tizon@gmail.com

