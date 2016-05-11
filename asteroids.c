#include <avr/io.h>
#include "lcd.h"
#include "math.h"
#include <util/delay.h>
#include <avr/interrupt.h>
#include <switches.h>
#include <stdlib.h>

#define PI 3.14159265

#define START_STATE 0
#define PLAY_STATE 1
#define END_STATE 2

#define DEAD 0
#define INVINCIBLE 1
#define VINCIBLE 2

#define NO_ASTEROIDS 12
#define NO_DEBRIS 10
#define NO_BULLETS 10
#define ASTEROID_PERIOD 64
#define TURN_ANGLE 0.23
#define ASTEROID_POINTS 4
#define SHIP_POINTS 8
#define SHIP_ACCELERATION 0.9
#define SHIP_DECELERATION 0.2
#define BIG_SIZE 40
#define MID_SIZE 30
#define SMALL_SIZE 15
#define TICK_MS 50
#define PENALTY 5

typedef struct {
	float x;
	float y;
} Point;

typedef struct {
	Point origin;
	Point points[SHIP_POINTS];
	
	uint8_t state;
	uint8_t thrusting;
	
	float x_inc;
	float y_inc;
	float x_dec;
	float y_dec;
	float angle;
	
	float horz_v;
	float vert_v;
} Ship;

typedef struct {
	uint16_t x;
	uint16_t y;
	int16_t x_inc;
	int16_t y_inc;
} Bullet;

typedef struct {
	uint16_t x;
	uint16_t y;
	int16_t x_inc;
	int16_t y_inc;
} Shrapnel;

typedef struct {
	Point origin;
	Point points[ASTEROID_POINTS];
	float x_inc;
	float y_inc;
	uint8_t size;
} Asteroid;

typedef struct {
	Point points[2];
	float x_inc;
	float y_inc;
} Wreckage;

void init();
void rotate_ship(float angle);
void handle_input();
void draw();
void update();
void thrust_ship();
void draw_asteroids();
void gen_asteroids();
void place_ship();
void init_pro();
void add_asteroid(float x, float y, float speed, float direction, float size);

uint8_t game_state = 0;
uint16_t score = 0;

Ship ship;
Ship last_ship;

// "Stack for the debris
Shrapnel debris[NO_DEBRIS];
uint8_t debris_l;
uint8_t debris_h;
Shrapnel last_debris[NO_DEBRIS];
uint8_t debris_last_l;
uint8_t debris_last_h;

// "Stack" for the bullets. 
Bullet bullets[NO_BULLETS];
uint8_t bullet_l;
uint8_t bullet_h;
Bullet last_bullets[NO_BULLETS];
uint8_t bullet_last_l;
uint8_t bullet_last_h;

// "Stack" for the asteroids; 
Asteroid asteroids[NO_ASTEROIDS+1];
Asteroid last_asteroids[NO_ASTEROIDS];
uint8_t asteroid_i;

Wreckage wreckages[4];
Wreckage last_wreckages[4];
uint8_t wreckage_life;

uint16_t inv_timer = 0;
uint16_t respawn_timer = 0;

void main(void) {
	init();
	sei();
	while (1) {}
}


ISR(TIMER1_COMPA_vect) {
	static uint8_t a_timer = ASTEROID_PERIOD;
	if (a_timer == 0) {
		gen_asteroids();
		a_timer = ASTEROID_PERIOD;
	}

	handle_input();
	update();
	draw();
   
	a_timer--;
}

void init() {	
	/* 8MHz clock, no prescaling (DS, p. 48) */
    CLKPR = (1 << CLKPCE);
    CLKPR = 0;

    init_lcd();
    init_switches();
    
    // Enable encoder reading pull-ups.
    DDRE &= ~_BV(PE4);
    PORTE |= _BV(PE4); 
    DDRE &= ~_BV(PE5);
    PORTE |= _BV(PE5);
	
	place_ship();
	
	bullet_l = 0;
	bullet_h = 0;
	bullet_last_l = 0;
	bullet_last_h = 0;
	
	debris_l = 0;
	debris_h = 0;
	debris_last_l = 0;
	debris_last_h = 0;

	asteroid_i = 0;
	
	/* Configure 16 bit Timer for ISR  */
    TCCR1B = _BV(WGM12)   /* Clear Timer on Compare match (CTC) Mode */
           | _BV(CS12)
           | _BV(CS10);   /* F_CPU / 1024 */

    OCR1A = (uint16_t)(F_CPU * TICK_MS / (1024.0 * 1000)- 0.5);

    TIMSK1 = _BV(OCIE1A); /* enable compare match interrupt */
    TCNT1 = 0;
}

/*
 * Rotates the point p about the point o.
 */
Point rotate_point (float sin_res, float cos_res, Point p, Point o) {
	float x = p.x - o.x;
	float y = p.y - o.y;
	float x_rot = x * cos_res - y * sin_res;
	float y_rot = x * sin_res + y * cos_res;
	
	p.x = x_rot + o.x;
	p.y = y_rot + o.y;
	
	return p;
}

/*
 * Rotates the space ship by the given angle.
 */
void rotate_ship (float angle) {
	float sin_result = sin(angle);
	float cos_result = cos(angle);
	
	ship.angle += angle;
	
	uint8_t i;
	for (i = 0; i < SHIP_POINTS; i++)
		ship.points[i] =
			rotate_point(sin_result, cos_result, ship.points[i], ship.origin);
	
	ship.x_inc = cos(ship.angle);
	ship.y_inc = sin(ship.angle);
	ship.x_dec = cos(ship.angle-3.14159265);
	ship.y_dec = sin(ship.angle-3.14159265);
}

/*
 *	Thrusts the space ship in the direction it is facing.
 */
void thrust_ship() {
	uint8_t i;
	for (i = 0; i < SHIP_POINTS; i++) {
		ship.points[i].x += ship.horz_v;
		ship.points[i].y += ship.vert_v;
	}
	ship.origin.x += ship.horz_v;
	ship.origin.y += ship.vert_v;
	
	// Wrap-around
	if (ship.origin.x > 350) {
		ship.origin.x -= 400;
		for (i = 0; i < SHIP_POINTS; i++)
			ship.points[i].x -= 400;
	}
	if (ship.origin.x < -30) {
		ship.origin.x += 400;
		for (i = 0; i < SHIP_POINTS; i++)
			ship.points[i].x += 400;
	}
	if (ship.origin.y > 270) {
		ship.origin.y -= 320;
		for (i = 0; i < SHIP_POINTS; i++)
			ship.points[i].y -= 320;
	}
	if (ship.origin.y < -30) {
		ship.origin.y += 320;
		for (i = 0; i < SHIP_POINTS; i++)
			ship.points[i].y += 320;
	}
}

/*
 * Checks whether the space ship is in collision with any of the asteroids,
 * resets the game and reduces the score if so.
 */
void check_collision() {
	if (ship.state != VINCIBLE)
		return;

	uint8_t i, j;
	for (i = 0; i < NO_ASTEROIDS; i++) {
		for (j = 0; j < SHIP_POINTS; j++) {
			if (ship.points[j].x > asteroids[i].origin.x
				&& ship.points[j].x < asteroids[i].origin.x + asteroids[i].size
				&& ship.points[j].y > asteroids[i].origin.y
				&& ship.points[j].y < asteroids[i].origin.y + asteroids[i].size) {
					float angle;
					uint8_t k;
					for (k = 0; k < 4; k++) {
						angle = ((float) rand()/ (float) RAND_MAX)*2*PI;
						wreckages[k].points[0] = ship.origin;
						wreckages[k].points[1].x = ship.origin.x + cos(angle)*10;
						wreckages[k].points[1].y = ship.origin.y + sin(angle)*10;
						angle = ((float) rand()/ (float) RAND_MAX)*2*PI;
						wreckages[k].x_inc = cos(angle)*2;
						wreckages[k].y_inc = sin(angle)*2;
						wreckage_life = 16;
					}
					ship.state = DEAD;
					score = (score>PENALTY)?score-PENALTY:0;
					respawn_timer = 0;
			}
		}
	}
}

/*
 * Adds an asteroid of a given size at position (x, y), heading in the given 
 * direction with the given speed.
 */ 
void add_asteroid(float x, float y, float speed, float direction, float size) {
	asteroids[asteroid_i].origin.x = x;
	asteroids[asteroid_i].origin.y = y;

	asteroids[asteroid_i].x_inc = speed * cos(direction);
	asteroids[asteroid_i].y_inc = speed * sin(direction);
	asteroids[asteroid_i].size = size;
	
	uint8_t i = 0;
	while (asteroids[asteroid_i].size != 0) {
		if (++i > NO_ASTEROIDS)
			break;
		asteroid_i = (asteroid_i+1)%NO_ASTEROIDS;
	}
}

/*
 * Adds some 'debris', the dots which scatter when an asteroid is destroided. 
 */
void add_debris(float x, float y) {
	float angle = ((float) rand()/ (float) RAND_MAX)*2*PI;
	debris[debris_h].x_inc = cos(angle)*5;
	debris[debris_h].y_inc = sin(angle)*10;
	debris[debris_h].x = x;
	debris[debris_h].y = y;
	debris_h = (debris_h + 1) % NO_DEBRIS;
}

/*
 * Places the ship in the center of the screen, facing up.
 */
void place_ship() {
	ship.origin.x = 160;
	ship.origin.y = 120;
	
	ship.points[0].x = ship.origin.x-6;
	ship.points[0].y = ship.origin.y+8;
	ship.points[1].x = ship.origin.x+6;
	ship.points[1].y = ship.origin.y+8;
	ship.points[2].x = ship.origin.x;
	ship.points[2].y = ship.origin.y-10;
	ship.points[3].x = ship.origin.x-5;
	ship.points[3].y = ship.origin.y+5;
	ship.points[4].x = ship.origin.x+5;
	ship.points[4].y = ship.origin.y+5;
	
	ship.points[5].x = ship.origin.x-3;
	ship.points[5].y = ship.origin.y+5;
	ship.points[6].x = ship.origin.x;
	ship.points[6].y = ship.origin.y+11;
	ship.points[7].x = ship.origin.x+3;
	ship.points[7].y = ship.origin.y+5;
	
	ship.angle = -PI/2; // Facing up.
	ship.x_inc = cos(ship.angle);
	ship.y_inc = sin(ship.angle);
	ship.x_dec = -ship.y_inc;
	ship.y_dec = -ship.x_inc;
	ship.state = INVINCIBLE;
	ship.horz_v = 0;
	ship.vert_v = 0;
	ship.thrusting = 0;
}

/*
 *	Draws the space ship.
 */
void draw_ship() {
	static uint8_t thrust_frame = 0;

	// Clear last position of ship.
	draw_line(last_ship.points[0].x, last_ship.points[0].y, last_ship.points[2].x, 
		last_ship.points[2].y, BLACK);
	draw_line(last_ship.points[2].x, last_ship.points[2].y, last_ship.points[1].x, 
		last_ship.points[1].y, BLACK);
	draw_line(last_ship.points[3].x, last_ship.points[3].y, last_ship.points[4].x, 
		last_ship.points[4].y, BLACK);
	draw_line(last_ship.points[5].x, last_ship.points[5].y, 
		last_ship.points[6].x, last_ship.points[6].y, BLACK);
	draw_line(last_ship.points[6].x, last_ship.points[6].y, 
		last_ship.points[7].x, last_ship.points[7].y, BLACK);
	
	if (ship.state == DEAD) 
		return;
	
	uint16_t ship_col = ship.state==INVINCIBLE?GREY:WHITE;
	draw_line(ship.points[0].x, ship.points[0].y, ship.points[2].x, 
		ship.points[2].y, ship_col);
	draw_line(ship.points[2].x, ship.points[2].y, ship.points[1].x, 
		ship.points[1].y, ship_col);
	draw_line(ship.points[3].x, ship.points[3].y, ship.points[4].x, 
		ship.points[4].y, ship_col);
	if (ship.thrusting && thrust_frame) {
		draw_line(ship.points[5].x, ship.points[5].y, 
			ship.points[6].x, ship.points[6].y, ship_col);
		draw_line(ship.points[6].x, ship.points[6].y, 
			ship.points[7].x, ship.points[7].y, ship_col);
		thrust_frame = 0;
	} else {
		thrust_frame = 1;
	}
	
	last_ship = ship;
}

/*
 * Draws all bullets present on the screen.
 */
void draw_bullets() {
	uint8_t i;
	for (i = bullet_last_l; i != bullet_last_h; i=(i+1)%NO_BULLETS) {
		draw_pixel(last_bullets[i].x, last_bullets[i].y, BLACK);
	}
	
	for (i = bullet_l; i != bullet_h; i=(i+1)%NO_BULLETS) {
		draw_pixel(bullets[i].x, bullets[i].y, WHITE);
	}
	
	bullet_last_l = bullet_l;
	bullet_last_h = bullet_h;
	for (i = bullet_l; i != bullet_h; i=(i+1)%NO_BULLETS) {
		last_bullets[i] = bullets[i];
	}
}

/*
 * Draws all debris present on the screen.
 */
void draw_debris() {
	uint8_t i;
	for (i = debris_last_l; i != debris_last_h; i=(i+1)%NO_DEBRIS) {
		draw_pixel(last_debris[i].x, last_debris[i].y, BLACK);
	}
	
	for (i = debris_l; i != debris_h; i=(i+1)%NO_DEBRIS) {
		draw_pixel(debris[i].x, debris[i].y, WHITE);
	}
	
	debris_last_l = debris_l;
	debris_last_h = debris_h;
	for (i = debris_l; i != debris_h; i=(i+1)%NO_DEBRIS) {
		last_debris[i] = debris[i];
	}
}

/*
 * Draws all asteroids present on the screen.
 */
void draw_asteroids() {
	uint8_t i;
	for (i = 0; i < NO_ASTEROIDS; i++) {
		if (asteroids[i].size > 0) {
			asteroids[i].origin.x+=asteroids[i].x_inc;
			asteroids[i].origin.y+=asteroids[i].y_inc;
			
			if (asteroids[i].origin.x > 320)
				asteroids[i].origin.x = -50;
			if (asteroids[i].origin.x < -50)
				asteroids[i].origin.x = 320;
			if (asteroids[i].origin.y > 240)
				asteroids[i].origin.y = -50;
			if (asteroids[i].origin.y < -50)
				asteroids[i].origin.y = 240;
		}
	}
	
	for (i = 0; i < NO_ASTEROIDS; i++) {
		if (last_asteroids[i].size > 0)
			draw_outline_rectangle(last_asteroids[i].origin.x, 
			last_asteroids[i].origin.y, 
				last_asteroids[i].size, last_asteroids[i].size, BLACK);
		else
			draw_outline_rectangle(last_asteroids[i].origin.x, 
			last_asteroids[i].origin.y, 
				SMALL_SIZE, SMALL_SIZE, BLACK);			
	}
	
	for (i = 0; i < NO_ASTEROIDS; i++)
		if (asteroids[i].size > 0)
			draw_outline_rectangle(asteroids[i].origin.x, asteroids[i].origin.y, 
				asteroids[i].size, asteroids[i].size, WHITE);

	for (i = 0; i < NO_ASTEROIDS; i++) {
		last_asteroids[i] = asteroids[i];
	}
}

/*
 * Draws all wreckage present on the screen.
 */
void draw_wreckage() {
	if (ship.state != DEAD) 
		return;

	uint8_t i;
	for (i = 0; i < 4; i++) {
		draw_line(last_wreckages[i].points[0].x, last_wreckages[i].points[0].y,
				last_wreckages[i].points[1].x, last_wreckages[i].points[1].y, BLACK);
	}

	if (wreckage_life == 0) 
		return;

	for (i = 0; i < 4; i++) {
		draw_line(wreckages[i].points[0].x, wreckages[i].points[0].y,
			wreckages[i].points[1].x, wreckages[i].points[1].y, WHITE);
		last_wreckages[i] = wreckages[i];
	}	
	
	wreckage_life--;
}

/*
 * Draws the game on the screen
 */
void draw() {
	display_thing_xy(10, 10, "Score: %d", score);
	draw_wreckage();
	draw_debris();
	draw_ship();
	draw_bullets();
	draw_asteroids();
	check_collision();
}

/*
 * Handles the player input.
 */
void handle_input() {
	if (center_pressed() && ship.state != DEAD) {
		bullets[bullet_h].x_inc = cos(ship.angle)*10;
		bullets[bullet_h].y_inc = sin(ship.angle)*10 ;
		bullets[bullet_h].x = ship.origin.x+bullets[bullet_h].x_inc;
		bullets[bullet_h].y = ship.origin.y+bullets[bullet_h].y_inc;
		bullet_h = (bullet_h + 1) % NO_BULLETS;
	}
	if (right_held() && ship.state != DEAD)
		rotate_ship(TURN_ANGLE);
	if (left_held() && ship.state != DEAD)
		rotate_ship(-TURN_ANGLE);
	if (up_held() && ship.state != DEAD) {
		ship.horz_v += SHIP_ACCELERATION*ship.x_inc;
		ship.vert_v += SHIP_ACCELERATION*ship.y_inc;
		ship.thrusting = 1;
	} else {
		ship.thrusting = 0;
	}
		
	float decel_angle = (ship.horz_v==0)?((ship.vert_v>0)?1.570896327:-1.570896327):atan(ship.vert_v/ship.horz_v);
	if (ship.horz_v < 0)
		decel_angle -= 3.14159265;
	float decel_hoz = cos(decel_angle + 3.14159265)*SHIP_DECELERATION;
	float decel_vert = sin(decel_angle + 3.14159265)*SHIP_DECELERATION;
	
	if (fabs(ship.horz_v) > fabs(decel_hoz))
		ship.horz_v += decel_hoz;
	else
		ship.horz_v = 0;
		
	if (fabs(ship.vert_v) > fabs(decel_vert))
		ship.vert_v += decel_vert;
	else
		ship.vert_v = 0;

	thrust_ship();
}

/*
 * Adds new asteroids to the game if there is space. The fully split asteroid
 * should not lead to more asteroids on screen than NO_ASTEROIDS.
 */
uint16_t pot_asteroids = 0; // Potential asteroids.
void gen_asteroids() {
	if (pot_asteroids < NO_ASTEROIDS-3) {
		uint16_t rnd = rand();
		add_asteroid(-30, -30, 3, ((float) rnd/ (float) RAND_MAX)*6, BIG_SIZE);
		pot_asteroids+=4;
	}
}

/*
 * Updates the state of the game.
 */
void update() {
	// Brief period of invincibility.
	if (ship.state == INVINCIBLE && ++inv_timer>32) {
		ship.state = VINCIBLE;
		inv_timer = 0;
	} else if (ship.state == DEAD && ++respawn_timer>32) {
		ship.state = INVINCIBLE;
		respawn_timer = 0;
		inv_timer = 0;
		place_ship();
	}

	uint8_t i, j;
	if (bullet_h - bullet_l != 0) {
		for (i = bullet_l; i != bullet_h; i=(i+1)%NO_BULLETS) {
			bullets[i].x+=bullets[i].x_inc;
			bullets[i].y+=bullets[i].y_inc;
			for (j = 0; j < NO_ASTEROIDS; j++) {
				// Checks if the bullets are in collision with asteroids by
				// checking the current position of the bullet and the 
				// half-way point between there and its last position (for
				// collision accuracy) 
				if (asteroids[j].size > 0 &&
						((bullets[i].x > asteroids[j].origin.x && 
						bullets[i].x < asteroids[j].origin.x + asteroids[j].size &&
						bullets[i].y > asteroids[j].origin.y &&
						bullets[i].y < asteroids[j].origin.y + asteroids[j].size) 
						|| (bullets[i].x-bullets[i].x_inc/2 > asteroids[j].origin.x && 
						bullets[i].x-bullets[i].x_inc/2 < asteroids[j].origin.x + asteroids[j].size &&
						bullets[i].y-bullets[i].y_inc/2 > asteroids[j].origin.y &&
						bullets[i].y-bullets[i].y_inc/2 < asteroids[j].origin.y + asteroids[j].size))) {
					if (asteroids[j].size == BIG_SIZE) {
						asteroids[j].size = 0;
						add_asteroid(asteroids[j].origin.x, 
							asteroids[j].origin.y, 5, ((float) rand()/ (float) RAND_MAX)*2*PI, MID_SIZE);
						add_asteroid(asteroids[j].origin.x, 
							asteroids[j].origin.y, 5, ((float) rand()/ (float) RAND_MAX)*2*PI, MID_SIZE);
					}
					else if (asteroids[j].size == MID_SIZE) {
						asteroids[j].size = 0;
						add_asteroid(asteroids[j].origin.x, 
							asteroids[j].origin.y, 5, ((float) rand()/ (float) RAND_MAX)*2*PI, SMALL_SIZE);
						add_asteroid(asteroids[j].origin.x, 
							asteroids[j].origin.y, 5, ((float) rand()/ (float) RAND_MAX)*2*PI, SMALL_SIZE);
					} else {
						asteroids[j].size = 0;
						pot_asteroids--;
					}
						
					bullet_l = (bullet_l+1)%NO_BULLETS;
					add_debris(asteroids[j].origin.x, asteroids[j].origin.y);
					add_debris(asteroids[j].origin.x, asteroids[j].origin.y);
					add_debris(asteroids[j].origin.x, asteroids[j].origin.y);
					add_debris(asteroids[j].origin.x, asteroids[j].origin.y);
					add_debris(asteroids[j].origin.x, asteroids[j].origin.y);
					
					score++;
					break;
				} 
			}
			
			if (bullets[i].x > 320 || bullets[i].y > 240)
				bullet_l = (bullet_l+1)%NO_BULLETS;
		}
	}
	
	for (i = debris_l; i != debris_h; i=(i+1)%NO_DEBRIS) {
			debris[i].x+=debris[i].x_inc;
			debris[i].y+=debris[i].y_inc;
			
			if (debris[i].x > 320 || debris[i].y > 240)
				debris_l = (debris_l+1)%NO_DEBRIS;
	}
	
	if (wreckage_life > 0) {
		for (i = 0; i < 4; i++) {
				wreckages[i].points[0].x+=wreckages[i].x_inc;
				wreckages[i].points[0].y+=wreckages[i].y_inc;
				wreckages[i].points[1].x+=wreckages[i].x_inc;
				wreckages[i].points[1].y+=wreckages[i].y_inc;
		}
	}
}




