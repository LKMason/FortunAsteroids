#include "switches.h"
#include <avr/io.h>

/*
 * Adapted from 'switches.c', from last year. I am unsure as the the original
 * author of it, it is present in a couple of projects (inc. TronFortuna and
 * FortunaTetris) 
 */

int center_down = 0;
int left_down = 0;
int right_down = 0;
int up_down = 0;
int down_down = 0;
int down = 0;

void init_switches(){
	DDRE &= ~_BV(SWC);
	PORTE |= _BV(SWC);
	DDRC &= ~_BV(SWN) & ~_BV(SWE) & ~_BV(SWS) & ~_BV(SWW);
	PORTC |= _BV(SWN) | _BV(SWE) | _BV(SWS) | _BV(SWW);

	DDRE &= ~_BV(SWC);   /* Central button */
	PORTE |= _BV(SWC);
	
	DDRC &= ~COMPASS_SWITCHES;  /* configure compass buttons for input */
	PORTC |= COMPASS_SWITCHES; 

	EICRB |= _BV(ISC40) | _BV(ISC50) | _BV(ISC71);
}

int center_pressed(){
	if ((~PINE & _BV(SWC)) && !center_down && !down) {
		center_down = 1;
		down = 1;
		return 1;
	} else if ((PINE & _BV(SWC)) && center_down){
		center_down = 0;
		down = 0;
	}
	return 0;
}

int up_pressed(){
	if ((~PINC & _BV(SWN)) && !left_down && !down) {
		left_down = 1;
		down = 1;
		return 1;
	} else if ((PINC & _BV(SWN))&& left_down){
		left_down = 0;
		down = 0;
	}
	return 0;
}

int down_pressed(){
	if ((~PINC & _BV(SWS)) && !right_down && !down) {
		right_down = 1;
		down = 1;
		return 1;
	} else if ((PINC & _BV(SWS)) && right_down){
		right_down = 0;
		down = 0;
	}
	return 0;
}

int right_pressed(){
	if ((~PINC & _BV(SWE)) && !up_down && !down) {
		up_down = 1;
		down = 1;
		return 1;
	} else if ((PINC & _BV(SWE)) && up_down){
		up_down = 0;
		down = 0;
	}
	return 0;
}

int left_pressed(){
	if ((~PINC & _BV(SWW)) && !down_down && !down) {
		down_down = 1;
		down = 1;
		return 1;
	} else if ((PINC & _BV(SWW)) && down_down){
		down_down = 0;
		down = 0;
	}
	return 0;
}

int right_held(){
	if ((~PINC & _BV(SWE)) && !down) {
		return 1;
		down = 1;
	} else {
		down = 0;
	}
	return 0;
}

int up_held(){
	if ((~PINC & _BV(SWN)) && !down) {
		return 1;
		down = 1;
	} else {
		down = 0;
	}
	return 0;
}

int left_held(){
	if ((~PINC & _BV(SWW)) && !down) {
		return 1;
		down = 1;
	} else {
		down = 0;
	}
	return 0;
}
