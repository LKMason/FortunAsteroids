#define SWN     PC2
#define SWE     PC3
#define SWS     PC4
#define SWW     PC5
#define SWC     PE7

#define COMPASS_SWITCHES (_BV(SWW)|_BV(SWS)|_BV(SWE)|_BV(SWN))


/* For La Fortuna board 

   | Port | Pin | Function                  |
   |------+-----+---------------------------|
   | C    |   2 | Directional Button North  |
   | C    |   3 | Directional Button East   |
   | C    |   4 | Directional Button South  |
   | C    |   5 | Directional Button West   |
   | E    |   7 | Centre Button				|
 
*/

void init_switches();
int center_pressed();
int left_pressed();
int right_pressed();
int up_pressed();
int down_pressed();
int down_held();
int up_held();
int left_held();
int right_held();
