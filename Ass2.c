// --------------------------------------------------
// ---   159.341 Assignment 2 - Lift Simulator    ---
// --------------------------------------------------
// Written by M. J. Johnson
// Updated by D. P. Playne
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "lift.h"

/*
The algorithm that each person should follow is: 
 
while(true) { 
   wait for a while (do some work) 
   pick a different floor to go to 
   if(going up) 
      press up arrow and wait 
   otherwise 
      press down arrow and wait 
   get into lift 
   press button for floor to go to 
   wait for lift to reach destination 
   get out of lift 
} 
 
The algorithm that each lift should follow is: 
 
 
while(true) { 
   drop off all passengers waiting to get out at this floor 
   if going up or empty 
      direction is up 
      pick up as many passengers on this floor going up as possible 
   if going down or empty 
      direction is down 
      pick up as many passengers on this floor going down as possible 
   move lift 
   if on top or ground floor 
      reverse direction 
} 
*/
// --------------------------------------------------
// Define Problem Size
// --------------------------------------------------
#define NLIFTS 4          // The number of lifts in the building
#define NFLOORS 20        // The number of floors in the building
#define NPEOPLE 20        // The number of people in the building
#define MAXNOINLIFT 10    // Maximum number of people in a lift


// --------------------------------------------------
// Define delay times (in milliseconds)
// --------------------------------------------------
#define SLOW
// #define FAST

#if defined(SLOW)
	#define LIFTSPEED 50      // The time it takes for the lift to move one floor
	#define GETINSPEED 50     // The time it takes to get into the lift
	#define GETOUTSPEED 50    // The time it takes to get out of the lift
	#define PEOPLESPEED 100   // The maximum time a person spends on a floor
#elif defined(FAST)
	#define LIFTSPEED 0       // The time it takes for the lift to move one floor
	#define GETINSPEED 0      // The time it takes to get into the lift
	#define GETOUTSPEED 0     // The time it takes to get out of the lift
	#define PEOPLESPEED 1     // The maximum time a person spends on a floor
#endif

// --------------------------------------------------
// Define lift directions (UP/DOWN)
// --------------------------------------------------
#define UP 1
#define DOWN -1

// --------------------------------------------------
// Information about a floor in the building
// --------------------------------------------------
typedef struct {
	int waitingtogoup;      // The number of people waiting to go up MUTEX?
	int waitingtogodown;    // The number of people waiting to go down
	semaphore up_arrow;     // People going up wait on this
	semaphore down_arrow;   // People going down wait on this
} floor_info;

// --------------------------------------------------
// Information about a lift
// --------------------------------------------------
typedef struct {
	int no;                       // The lift number (id)
	int position;                 // The floor it is on
	int direction;                // Which direction it is going (UP/DOWN)
	int peopleinlift;             // The number of people in the lift
	int stops[NFLOORS];           // How many people are going to each floor
	semaphore stopsem[NFLOORS];   // People in the lift wait on one of these <- this is a semephore for each floor
} lift_info;

// --------------------------------------------------
// Some global variables
// --------------------------------------------------
floor_info floors[NFLOORS];
lift_info* current_lift; //Variable to store the current lift
semaphore floor_lock[NLIFTS]; //Lock for each floor
semaphore print_lock; ////Lock for printing to the screen


// --------------------------------------------------
// Print a string on the screen at position (x,y)
// --------------------------------------------------
void print_at_xy(int x, int y, const char *s) {
	semaphore_wait(&print_lock); //Wait for the lock to be free

	// Check coordinates
    if (x < 0 || x > 80 || y < 0 || y > NFLOORS+2) {
        printf("ERROR: Invalid print coordinates (%d,%d)\n", x, y);
        semaphore_signal(&print_lock);
        return;
    }
	
	gotoxy(x,y);
	
	// Slow things down
	Sleep(1);

	// Print the string
	printf("%s", s);
	
	// Move cursor out of the way
	gotoxy(42, NFLOORS+2);

	semaphore_signal(&print_lock);//Signal when complete

}

// --------------------------------------------------
// Function for a lift to pick people waiting on a
// floor going in a certain direction
// --------------------------------------------------
void get_into_lift(lift_info *lift, int direction) {
	// Local variables
	int *waiting;
	semaphore *s;

	// Check lift direction
	if(direction==UP) {
		// Use up_arrow semaphore
		s = &floors[lift->position].up_arrow;

		// Number of people waiting to go up
		waiting = &floors[lift->position].waitingtogoup;
	} else {
		// Use down_arrow semaphore
		s = &floors[lift->position].down_arrow;

		// Number of people waiting to go down
		waiting = &floors[lift->position].waitingtogodown;
	}

	// For all the people waiting
	while(*waiting) {
		// Check if lift is empty
		if(lift->peopleinlift == 0) {
			// Set the direction of the lift
			lift->direction = direction;
		}
		
		// Check there are people waiting and lift isn't full
		if(lift->peopleinlift < MAXNOINLIFT && *waiting) {
			// Add person to the lift
			lift->peopleinlift++;

			// Erase the person from the waiting queue
			print_at_xy(NLIFTS*4+floors[lift->position].waitingtogodown + floors[lift->position].waitingtogoup, NFLOORS-lift->position, " ");

			// One less person waiting
			(*waiting)--;

			// Wait for person to get into lift
			Sleep(GETINSPEED);

//---			// Set which lift the passenger should enter
current_lift = lift;
		
//---			// Signal passenger to enter
semaphore_signal(s);
// When a person gets into a lift:
//printf("Person entering lift %d to go to floor %d direction %d\n", lift->no, lift->position, lift->direction);//debug
			// Print the lift number
			//print_at_xy(lift->no*4+1, NFLOORS-lift->position, (lift->direction + 1 ? " " : lc));
		 } else {
			break;
		}
	}
}

// --------------------------------------------------
// Function for the Lift Threads
// --------------------------------------------------
void* lift_thread(void *p) {
	// Local variables
	lift_info lift;

	unsigned long long int no = (unsigned long long int)p;
	int i;
	
	// Set up Lift
	lift.no = no;           // Set lift number
	lift.position = 0;      // Lift starts on ground floor
	lift.direction = UP;    // Lift starts going up
	lift.peopleinlift = 0;  // Lift starts empty

	for(i = 0; i < NFLOORS; i++) {
		lift.stops[i]=0;                        // No passengers waiting
		semaphore_create(&lift.stopsem[i], 0);  // Initialise semaphores
	}

	// Randomise lift
	randomise();

	// Wait for random start up time (up to a second)
	Sleep(rnd(1000));

	// Loop forever
	while(true) 
	{
		// Print current position of the lift
		print_at_xy(no*4+1, NFLOORS-lift.position, lf);

		// Wait for a while
		Sleep(LIFTSPEED);

		// Drop off passengers on this floor
		while (lift.stops[lift.position] != 0) {
			// One less passenger in lift
			lift.peopleinlift--;

			// One less waiting to get off at this floor
			lift.stops[lift.position]--;
			
			// Wait for exit lift delay
			Sleep(GETOUTSPEED);

//---			// Signal passenger to leave lift	
			semaphore_signal(&lift.stopsem[lift.position]);
			
//debug 
//printf("Lift %d at position %d with direction %d\n", lift.no, lift.position, lift.direction);
			// Check if that was the last passenger waiting for this floor
			if(!lift.stops[lift.position]) {
				// Clear the "-"
				print_at_xy(no*4+1+2, NFLOORS-lift.position, " ");
			}
		}
		// Check if lift is going up or is empty
		if(lift.direction==UP || !lift.peopleinlift) {
			// Pick up passengers waiting to go up
			get_into_lift(&lift, UP);
		}
		// Check if lift is going down or is empty
		if(lift.direction==DOWN || !lift.peopleinlift) {
			// Pick up passengers waiting to go down
			get_into_lift(&lift, DOWN);
		}
		
		// Erase lift from screen
		print_at_xy(no*4+1, NFLOORS-lift.position, (lift.direction + 1 ? " " : lc));

		// Move lift
		lift.position += lift.direction;

		// Check if lift is at top or bottom
		if(lift.position == 0 || lift.position == NFLOORS-1) {
			// Change lift direction
			lift.direction = -lift.direction;
		}
	}
	
	return NULL;
}

// --------------------------------------------------
// Function for the Person Threads
// --------------------------------------------------
void* person_thread(void *p) {
	// Local variables
	int from = 0, to; // Start on the ground floor
	lift_info *lift;

	// Randomise
	randomise();

	// Stay in the building forever
	while(1) {
		// Work for a while
		Sleep(rnd(PEOPLESPEED));
		do {
			// Randomly pick another floor to go to
			to = rnd(NFLOORS);
		} while(to == from);

		// Check which direction the person is going (UP/DOWN)
		if(to > from) {
			// One more person waiting to go up
			floors[from].waitingtogoup++;
			
			// Print person waiting
			print_at_xy(NLIFTS*4+ floors[from].waitingtogoup +floors[from].waitingtogodown,NFLOORS-from, pr);
			
///---			// Wait for a lift to arrive (going up)
semaphore_wait(&floors[from].up_arrow);
		} else {
			// One more person waiting to go down
			floors[from].waitingtogodown++;
			
			// Print person waiting
			print_at_xy(NLIFTS*4+floors[from].waitingtogodown+floors[from].waitingtogoup,NFLOORS-from, pr);
			
///---			// Wait for a lift to arrive (going down)
semaphore_wait(&floors[from].down_arrow);
		}
		
		// Which lift we are getting into
		///---
	lift = current_lift;
		// Print the lift number
		//Debugit
		//printf("Person going from floor %d to floor %d\n", from, to);

		// Add one to passengers waiting for floor
		lift->stops[to]++;

		// Press button if we are the first
		if(lift->stops[to]==1) {
			// Print light for destination
			print_at_xy(lift->no*4+1+2, NFLOORS-to, "-");
		}

///---		// Wait until we are at the right floor
semaphore_wait(&lift->stopsem[to]);
		// Print the lift number
		print_at_xy(lift->no*4+1, NFLOORS-to, (lift->direction + 1 ? " " : lc));

		// Exit the lift
		from = to;
	}
	
	return NULL;
}

// --------------------------------------------------
//	Print the building on the screen
// --------------------------------------------------
void printbuilding(void) {
	// Local variables
	int l,f;

	// Clear Screen
	system(clear_screen);
	
	// Print Roof
	printf("%s", tl);
	for(l = 0; l < NLIFTS-1; l++) {
		printf("%s%s%s%s", hl, td, hl, td);
	}
	printf("%s%s%s%s\n", hl, td, hl, tr);

	// Print Floors and Lifts
	for(f = NFLOORS-1; f >= 0; f--) {
		for(l = 0; l < NLIFTS; l++) {
			printf("%s%s%s ", vl, lc, vl);
			if(l == NLIFTS-1) {
				printf("%s\n", vl);
			}
		}
	}

	// Print Ground
	printf("%s", bl);
	for(l = 0; l < NLIFTS-1; l++) {
		printf("%s%s%s%s", hl, tu, hl, tu);
	}
	printf("%s%s%s%s\n", hl, tu, hl, br);

	// Print Message
	printf("Lift Simulation - Press CTRL-Break to exit\n");
}

// --------------------------------------------------
// Main starts the threads and then waits.
// --------------------------------------------------
int main() {
	// Local variables
	unsigned long long int i;
	semaphore_create(&print_lock, 1);
	
	// Initialise random number generator
	randomise();

	// Initialise Building
	for(i = 0; i < NFLOORS; i++) {
		// Initialise Floor
		floors[i].waitingtogoup = 0;
		floors[i].waitingtogodown = 0;
		semaphore_create(&floors[i].up_arrow, 0);
		semaphore_create(&floors[i].down_arrow, 0);
		
	}

	// --- Initialise any other semaphores ---
	
	// Print Building
	printbuilding();

	for(i = 0; i < NLIFTS; i++) {
		// Create Lift Thread
		semaphore_create(&floor_lock[i], 1);
		printf("Semaphore created\n");
	}
	

	// Create Lifts
	for(i = 0; i < NLIFTS; i++) {
		// Create Lift Thread
		create_thread(lift_thread, (void*)i);
	}

	// Create People
	for(i = 0; i < NPEOPLE; i++) {
		// Create Person Thread
		create_thread(person_thread, (void*)i);
	}

	// Go to sleep for 86400 seconds (one day)
	Sleep(86400000ULL);
}


