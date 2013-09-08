/// DDS sinus frequency generator
// (c) 2013 Kristoff Bonne (ON1ARF)

// 10 aug. 2013

// development code provided for education purposes. License: GPL

// DEBUG. Uncomment to make active
#define DEBUG

// DEFINES
#define SAMPLERATE 48000
#define SAMPLERATE_F 48000.0F


// for uint32_T
#include <stdint.h>

// for read, write
#include <unistd.h>

// for exit
#include <stdlib.h>

// for EAGAIN
#include <errno.h>

// for sin
#include <math.h>

// for stderr
#include <stdio.h>


// GLOBAL constants
const float pi = 3.141592653589793;


// GLOBAL DATA
int32_t tlow;
int32_t thigh;
int bps;


// functions defined below
void generate_and_write (unsigned char in, int fd);

int main (int argc, char ** argv) {
int c1; // counters
double lowfreqf, highfreqf;

unsigned char charin;
int ret;

unsigned char learn=0x33;

unsigned char syncpattern[] = {
	0x81, 0x41, 0x21 // = 0x81, 0x82, 0x84, bit order returned
}; 

if (argc < 4) {
	fprintf(stderr,"Error: need at least 3 vars: bps, lowfreq, highfreq\n");
	exit(-1);
}; // end if

bps=atoi(argv[1]);

lowfreqf=atof(argv[2]);
highfreqf=atof(argv[3]);



tlow=(int)((4294967296.0F*lowfreqf/SAMPLERATE_F)+0.5);
thigh=(int)((4294967296.0F*highfreqf/SAMPLERATE_F)+0.5);




//////// create audio

// DEBUG
// part 0a: 3 second of 0 (1000 Hz)
for (c1=0; c1<675; c1++) {
	generate_and_write(0x00,1); // write to stdout
}; // end for


// part 0b: 3 second of 1 (2400 Hz)
for (c1=0; c1<675; c1++) {
	generate_and_write(0xff,1); // write to stdout
}; // end for


// part 1: 500 ms (minimum 900 bits) learning pattern 0b00110011
// we send 113 octets (=904 bits)

for (c1=0; c1<113; c1++) {
	generate_and_write(learn,1); // write to stdout
}; // end for


// write frame-sync pattern
for (c1=0; c1<3; c1++) {
	generate_and_write(syncpattern[c1],1); // write to stdout
}; // end for


// read from stdin
ret=read(0,&charin,1);

while ((ret > 0) || (ret == EAGAIN)) {
	if (ret > 0) {
		generate_and_write(charin,1); // write to stdout
	}; // end if

	ret=read(0,&charin,1);
}; // end for (c1)


return(0);
}; 


// end main application



// functions

///////////
// generate_and_write


void generate_and_write (unsigned char in, int fd) {
static int32_t audio=0;
static int samplecount=0;

int16_t audio16;

static int16_t s[65536];
static int init=1;

int c1; // counters

int ret;

// init vars
if (init) {
	// fill up table
	for (c1=0; c1 < 65536; c1++) {
		s[c1]=(int16_t) (sin((c1*2*pi/65536)+.5)*32768);
	}; // end for

init=0;
}; // end if

// if one
for (c1=0; c1<8; c1++) {
	if (in & 0x80) {
		// if one
		do {
			audio += thigh;
			audio16 = (audio >> 16);
#ifdef DEBUG
fprintf(stderr,"BIT 1 audio = %08X; audio16 = %08Xd \n",audio,audio16);
#endif

			ret=write(fd,&s[audio16+32768],sizeof(int16_t));
			samplecount += bps;
		} while (samplecount < SAMPLERATE);
		samplecount -= SAMPLERATE;

	} else {
		// if zero
		do {
			audio += tlow;
			audio16 = (audio >> 16);
#ifdef DEBUG
fprintf(stderr,"BIT 0 audio = %08X; audio16 = %08Xd \n",audio,audio16);
#endif

			ret=write(fd,&s[audio16+32768],sizeof(int16_t));
			samplecount += bps;
		} while (samplecount < SAMPLERATE);
		samplecount -= SAMPLERATE;
	}; // end if

	// move up
	in <<= 1;

}; // end for



// done
return;

}; // end function generate_and_write
