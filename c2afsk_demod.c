// C2AFSK decoder

// (c) kristoff Bonne (ON1ARF)

// 8 sept. 2013

// development code provided for education purposes. License: GPL


// baudrate = 1800, 
// audio 48 Khz samplingrate
// mark = 1000 Hz, centre freq = 1700 Hz, space = 2400 Hz

// debug, uncomment to make active
#define DEBUG


// for sin and cos
#include <math.h>

// for abs
#include <stdlib.h>

// for memmove
#include <string.h>

// for int16_t
#include <stdint.h>

// for read
#include <unistd.h>

// for printf
#include <stdio.h>


// DEFINES

#define FOREVER 1

#define UNDERSAMP 1
#define CORRLEN ((int)(48000/1800/UNDERSAMP))

#define PHASEINC (0x10000*1800*UNDERSAMP/48000)
#define SMALLPHASEINC (0x2000*1800*UNDERSAMP/48000) // 1/8 of PHASEINC

#define SYNCPATTERN 0x814121

// tables for i and q of "mark" frequency
float mark_i[CORRLEN];
float mark_q[CORRLEN];

// tables for i and q of "space" frequency
float space_i[CORRLEN];
float space_q[CORRLEN];


// audio buffer
float audiobuffer[CORRLEN];

// oversampling counter
int oversamp;

// size of buffer to move for new data
const int mem2move=sizeof(float)*(CORRLEN-1);



//////////////////////////////////////////////
// function init vars
void initvars () {
	int l;
	float f; float step;

	
	// for "MARK" freq (1000 Hz)
	f=0; step = 2.0*M_PI*1000*UNDERSAMP/48000;
	for (l=0; l < CORRLEN; l++) {
		mark_i[l] = cos(f);
		mark_q[l] = sin(f);
		f += step;
	}; // end if

	// for "SPACE" freq (2400 Hz)
	f=0; step = 2.0*M_PI*2400*UNDERSAMP/48000;
	for (l=0; l < CORRLEN; l++) {
		space_i[l] = cos(f);
		space_q[l] = sin(f);
		f += step;
	}; // end if

	// init oversampling counter
	oversamp=0;

	// init audiobuffer buffer
	memset(audiobuffer,0,CORRLEN * sizeof(float));

	return;
}; // end function init vars

//////////////////////////////////////////////
// function fsqr
float fsqr(float f) {
	return(f*f);
}; // end fnction float square


//////////////////////////////////////////////
// function multiply and accumulate
static inline float multiply_and_accumulate(const float *b1, const float *b2, unsigned int size)
{
	float sum = 0;
	unsigned int l;

	for (l = 0; l < size; l++) {
		sum += (*b1++) * (*b2++);
	}; // end for

	return(sum);
}


////////////////////////////////////////////////
// function DO PLL
// bit (old bit and new bit) and phase are copied from main application
// new bit is returned in "bit"
int dopll (float f, int *lastbit, int *phase) {
// local var
int newbit;
int pllshift;

if (f > 0) {
	newbit=0;
} else {
	newbit=1;
}; // end if


if ((newbit != *lastbit) && (*lastbit != -1)) {
	// PLL correction. Bit change. move window left of right
	if (*phase < 0x8000) {
		// window to much to the left, move right
		pllshift=1;

		*phase += SMALLPHASEINC;
	} else {
		// window to much to the right, move left
		pllshift=-1;

		*phase -= SMALLPHASEINC;
	}; // end else - if
} else {
	pllshift=0;
}; // end if

#ifdef DEBUG
printf("f= %+12.3f, bit = %d, newbit = %d, phase = %5d ",f,*lastbit,newbit,*phase);
#endif

// store bit
*lastbit=newbit;
*phase += PHASEINC;


if (pllshift == 1) {
	printf("> ");
} else if (pllshift == -1) {
	printf("< ");
}; // end if

// return bit, if phase has made full circle (over 0x10000)
if (*phase < 0x10000) {
	printf("\n");
	return(-2); // return -2 if not a descisive audio sample
}; // end if


*phase -= 0x10000;
return(newbit);

}; // end function PLL
/////////////////////////////////////////////////

// function "process audioin"
int process_audioin(int16_t audioin, int *in_state) {
static int init=1;
static int sampcount=0;

int ret;

static uint32_t last32bits, last32bits_att, last32bits_noatt;
 

static int state=0;

static int lastbit=-1;
static int phase=0;

// data for training mode
static int lastbit_att=-1;
static int phase_att=0;

static int lastbit_noatt=-1;
static int phase_noatt=0;


static float signal_mark, signal_att_mark, signal_noatt_mark;
static float signal_space, signal_att_space, signal_noatt_space;
static int count_att_mark=0, count_att_space=0, count_noatt_mark=0, count_noatt_space=0;

float f1,f2,f;

static int tolowcount=0;

static float attfact; // attenunation factor (init = 4 = 6db)


if (init) {
	// executed the first time the function is called

	// global vars
	initvars();

	
	// local vars
	state=0;
	last32bits_att=0; last32bits_noatt=0;

	attfact=4; // 6db = factor 4

	// end of init
	init=0;
}; // end "init"




// check for oversampling
sampcount++;

if (sampcount < UNDERSAMP) {
	*in_state=-1;
	return(-1);
}; // end return

// reset oversamp
sampcount=0;

// insert data in buffer
// first move down data in buffer
memmove(audiobuffer,&audiobuffer[1],mem2move);
// store data at end of buffer
audiobuffer[CORRLEN-1]=((float)audioin)/32768;

// calculate energy:
// energy at "mark" frequency
f1=fsqr(multiply_and_accumulate(audiobuffer,mark_i,CORRLEN)) +
	fsqr(multiply_and_accumulate(audiobuffer,mark_q,CORRLEN));

// energy at "space" frequency
f2=fsqr(multiply_and_accumulate(audiobuffer,space_i,CORRLEN)) +
	fsqr(multiply_and_accumulate(audiobuffer,space_q,CORRLEN));

// ignore samples if level to low
if ((fabs(f1) < 0.1) && (fabs(f2) < 0.1)) {
	*in_state=-1;

	tolowcount++;

	// more then 200 ms of to-low. Reset data
	
	if (tolowcount*UNDERSAMP > 4800) {
#ifdef DEBUG
printf("TOLONG TOLOW! \n");
#endif
		tolowcount=0;

		signal_att_mark=0; count_att_mark=0;
		signal_att_space=0; count_att_space=0;

		signal_noatt_mark=0; count_noatt_mark=0;
		signal_noatt_space=0; count_noatt_space=0;

		last32bits_att=0; last32bits_noatt=0;

		lastbit_att=0; phase_att=0; lastbit_noatt=0; phase_noatt=0;

	}; // end if

	return(-5);
} else {
	tolowcount=0;
}; // end if



// state machine:
// state = 0 -> learning
// state = 1 -> waiting for sync
// state = 2 -> receiving

// in learning mode
if (state==0)  {
// senario one: SPACE is "attfact" db attentuated to MARK
	ret=dopll (f1-f2*attfact, &lastbit_att, &phase_att);

	if (ret == 0) {
#ifdef DEBUG
printf("BIT0 ");
#endif
		signal_att_mark+=f1;
		count_att_mark++;

		last32bits_att<<=1;

	} else if (ret == 1) {
#ifdef DEBUG
printf("BIT1 ");
#endif
		signal_att_space+=f2;
		count_att_space++;

		last32bits_att<<=1;
		last32bits_att |= 1;
	}; // end if	

#ifdef DEBUG
printf("ATT ret = %0d, f1 = %+2.5f, f2 = %+2.5f, attfact = %f, count_att_mark = %d, count_att_space = %d \n",ret, f1, f2*attfact, attfact, count_att_mark,count_att_space);
#endif

	if (abs(count_att_mark - count_att_space) >= 10) {

#ifdef DEBUG
printf("ATT RESET \n");
#endif
		// set attenuation factor to current average value
		if (count_att_mark > count_att_space * 1.4) {
#ifdef DEBUG
printf("ATT RESET ... TO MANY MARK! \n");
#endif
			attfact *= 1.25;

		} else if (count_att_space > count_att_mark * 1.4) {
#ifdef DEBUG
printf("ATT RESET ... TO MANY SPACE! \n");
#endif
			attfact /= 1.3;

		}; // end else - elseif - if

			// RESET counters
			signal_att_mark=0; count_att_mark=0;
			signal_att_space=0; count_att_space=0;

			last32bits_att=0;


			
	}; // end if

	// do we have 40 zeros and 40 ones ?
	if ((count_att_mark >= 40) && (count_att_space >= 40)) {

		signal_mark=signal_att_mark / count_att_mark;
		signal_space=signal_att_space / count_att_space;

		// store values of "lastbit" and "phase"
		lastbit=lastbit_att;
		phase=phase_att;

		last32bits=last32bits_att;

		attfact = (signal_mark / signal_space);
#ifdef DEBUG
printf("TRAINING-DONE-ATT: s_m = %+2.7f, s_s=%+2.7f, attfact=%+2.7f \n",signal_mark, signal_space, attfact);
#endif

		// end learning fase
		state=1;

		*in_state=1;
		return(-3);
	}; // end if
	
// senario two: SPACE is not attenuated to MARK
	ret=dopll(f1-f2, &lastbit_noatt, &phase_noatt);

	if (ret == 0) {
#ifdef DEBUG
printf("BIT0 ");
#endif
		signal_noatt_mark+=f1;
		count_noatt_mark++;

		last32bits_noatt <<=1;

	} else if (ret == 1) {
#ifdef DEBUG
printf("BIT1 ");
#endif
		signal_noatt_space+=f2;
		count_noatt_space++;

		last32bits_noatt <<=1;
		last32bits_noatt |= 1;

	}; // end if	

#ifdef DEBUG
printf("NOA ret = %0d, f1 = %+2.5f, f2 = %+2.5f, count_noa_mark = %d, count_noa_space = %d \n",ret, f1, f2, count_noatt_mark,count_noatt_space);
printf("\n");
#endif

	// reset counters if unbalance is to large (more then 30)
	if (abs(count_noatt_mark - count_noatt_space) >= 30) {
			// RESET counters
#ifdef DEBUG
printf("NOA RESET! \n");
#endif
			signal_noatt_mark=0; count_noatt_mark=0;
			signal_noatt_space=0; count_noatt_space=0;

			last32bits_noatt=0;
			
	}; // end if

	// do we have 40 zeros and 40 ones ?
	if ((count_noatt_mark >= 40) && (count_noatt_space >= 40) ) {

		signal_mark=signal_noatt_mark / count_noatt_mark;
		signal_space=signal_noatt_space / count_noatt_space;

		// store values of "lastbit" and "phase"
		lastbit=lastbit_noatt;
		phase=phase_noatt;

		last32bits=last32bits_noatt;

		// end learning fase
		state=1;

#ifdef DEBUG
printf("TRAINING-DONE-NOA: s_m = %+2.7f, s_s=%+2.7f\n",signal_mark, signal_space);
#endif

		// set attenuation factor to 1
		attfact = 1;

		*in_state=1;
		return(-3);
	}; // end if
	

	// not yet 50 0's and 50 1's for either senario
	// return
	*in_state=0;
	return(-4);
}; // end if (state 0: learning)


// learning is done. Decoding using correction parameters
// calculated during learning
//f=(f1-noise_mark)-(f2-noise_space)*signal_mark/signal_space;
f=(f1-(f2*attfact));
ret=dopll(f,&lastbit,&phase);
#ifdef DEBUG
printf("ret = %0d, fcor = %+2.7f, f-noncorr = %+2.7f, f1 = %+2.7f, f2 = %+2.7f, f2corr = %+2.7f\n",ret, f, f1-f2, f1, f2,f2*attfact);
#endif

// not retured a valid bit.
if (ret < 0) {
	*in_state=state;
	return(ret);
}; // end 

#ifdef DEBUG
printf("BIT%0d, fcor = %+2.7f, f-noncorr = %+2.7f, f1 = %+2.7f, f2 = %+2.7f\n",(ret&0x01), f, f1-f2, f1, f2);
#endif

// received valid bit
// copy bit to "last32bits"
last32bits<<=1;
last32bits |= (ret & 0x1);

// state: 1: waiting for sync
if (state == 1) {
	// look for "sync" in pattern

#ifdef DEBUG
	printf("last32bits = %08x - SYNC PATTERN = %08X \n",(last32bits&0xffffff),SYNCPATTERN);
#endif
	
	if ((last32bits & 0x00ffffff) == SYNCPATTERN) {
		// sync found

		// move to state 2(receiving)
		state=2;
		*in_state=2;
		return(-7);
	};

	// sync not found
	return(-8);

}; // end if (state 1: waiting for sync)


// state 2: receiving
// just return data
*in_state=2;
return(ret);

}; // end function process_audioin




// MAIN APPLICATION

int main (int argc, char ** argv) {
int r, n;
int state;

int16_t in;
// read data from standardin

int bitcount;
unsigned char c;


bitcount=0;


while (FOREVER) {
	n=read(0,&in,sizeof(int16_t));

	if (n < sizeof(int16_t)) {
		break;
	}; // end

	r=process_audioin(in,&state);
#ifdef DEBUG
printf("state = %d \n",state);
#endif

	if (r >= 0) {
		c <<= 1;
		if (r) {
			c |= 1;
		}; // end if

		bitcount++;

		if (bitcount >= 8) {
			printf("c = %02X \n",(c & 0xff));
write(2,&c,1);
			bitcount=0;
			c=0x00;
		}; // end if

	}; // end if
}; // end while

#ifdef DEBUG
printf("END ... bitcount = %d \n",bitcount);
printf("c = %02X \n",(c & 0xff));
#endif


return(0);

}; // END MAIN APPLICATION
