// --------------------------------------------------
// ---   159.341 Assignment 2 - Lift Simulator    ---
// --------------------------------------------------
// Written by M. J. Johnson
// Updated by D. P. Playne
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include "lift.h"
#include <stdbool.h> // Added for bool compatibility

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
// Boolean type would not compile on Windows without

// typedef unsigned char bool;
// #define true 1
// #define false 0

//

// --------------------------------------------------
// Define Problem Size
// --------------------------------------------------
#define NLIFTS 4	   // The number of lifts in the building
#define NFLOORS 20	   // The number of floors in the building
#define NPEOPLE 20	   // The number of people in the building
#define MAXNOINLIFT 10 // Maximum number of people in a lift

// --------------------------------------------------
// Define delay times (in milliseconds)
// --------------------------------------------------
//#define SLOW
 #define FAST

#if defined(SLOW)
#define LIFTSPEED 50	// The time it takes for the lift to move one floor
#define GETINSPEED 50	// The time it takes to get into the lift
#define GETOUTSPEED 50	// The time it takes to get out of the lift
#define PEOPLESPEED 100 // The maximum time a person spends on a floor
#elif defined(FAST)
#define LIFTSPEED 0	  // The time it takes for the lift to move one floor
#define GETINSPEED 0  // The time it takes to get into the lift
#define GETOUTSPEED 0 // The time it takes to get out of the lift
#define PEOPLESPEED 1 // The maximum time a person spends on a floor
#endif

// --------------------------------------------------
// Define lift directions (UP/DOWN)
// --------------------------------------------------
#define UP 1
#define DOWN -1

// --------------------------------------------------
// Information about a floor in the building
// --------------------------------------------------
typedef struct
{
	int waitingtogoup;	  // The number of people waiting to go up MUTEX?
	int waitingtogodown;  // The number of people waiting to go down
	semaphore up_arrow;	  // People going up wait on this
	semaphore down_arrow; // People going down wait on this
} floor_info;

// --------------------------------------------------
// Information about a lift
// --------------------------------------------------
typedef struct
{
	int no;						// The lift number (id)
	int position;				// The floor it is on
	int direction;				// Which direction it is going (UP/DOWN)
	int peopleinlift;			// The number of people in the lift
	int stops[NFLOORS];			// How many people are going to each floor
	semaphore stopsem[NFLOORS]; // People in the lift wait on one of these <- this is a semephore for each floor
} lift_info;

// --------------------------------------------------
// Some global variables
// --------------------------------------------------
floor_info floors[NFLOORS];
lift_info *current_lift; // to store the current lift <- Shows comms between threads
semaphore floor_lock;	 // Lock for each floor //Lecture 16 - Mutex
semaphore lift_lock;	 ////Lock for lift data access /Lecture 16 - Mutex
semaphore print_lock;	 // Lock for printing to the screen <- Preventing output from being munched

// Debug
int count_Semphores = 0; // Count the number of semaphores created

// --------------------------------------------------
// Print a string on the screen at position (x,y)
// --------------------------------------------------
void print_at_xy(int x, int y, const char *s)
{

	// Lets first check if the Cords are within bounds this will prefent error in terminal, print to console if out of bounds
	if (x < 0 || y < 0)
	{
		printf("ERROR: Invalid print coordinates (%d,%d)\n", x, y);
		return;
	}

	// Wait for the print lock and implementing mutual exclusion
	semaphore_wait(&print_lock); // Wait for the lock to be free

	// DEBUG : Count the number of semaphore operations
	count_Semphores++;
	// if (count_Semphores % 100 == 0)
	// {
	// 	printf("DEBUG: Semaphore operations: %d\n", count_Semphores);
	// }

	// Move cursor to X,Y
	// printf("Printing at (%d,%d)\n", x, y);
	gotoxy(x, y);

	// Slow
	Sleep(1);

	// Print the string
	printf("%s", s);

	// Move cursor out of the way
	gotoxy(42, NFLOORS + 2);

	// Release the lock <- Always release after aquiring -> Lecture 16/17
	semaphore_signal(&print_lock);
}
void print_information()
{
	printf("---------------------------------------------------\n");
	printf("-------159.341 Assignment 2 Semester 1 2025 -------\n");
	printf("---------Submitted by: Zak Turner 23004797---------\n");
	printf("---------------------------------------------------\n");
}

// --------------------------------------------------
// Function for a lift to pick people waiting on a
// floor going in a certain direction
// --------------------------------------------------
void get_into_lift(lift_info *lift, int direction)
{
	// Local variables
	int *waiting;
	semaphore *s;

	// Check lift direction
	if (direction == UP)
	{
		// Use up_arrow semaphore
		s = &floors[lift->position].up_arrow;

		// Number of people waiting to go up
		waiting = &floors[lift->position].waitingtogoup;
		// printf("DEBUG: Lift %d at floor %d going UP\n", lift->no, lift->position);
	}
	else
	{
		// Use down_arrow semaphore
		s = &floors[lift->position].down_arrow;

		// Number of people waiting to go down
		waiting = &floors[lift->position].waitingtogodown;
		// printf("DEBUG: Lift %d at floor %d going DOWN\n", lift->no, lift->position);
	}

	// For all the people waiting
	while (*waiting)
	{
		// Check if lift is empty
		if (lift->peopleinlift == 0)
		{
			// Set the direction of the lift
			lift->direction = direction;
			if (direction == UP)
			{
				// printf("DEBUG: empty Lift %d going UP\n", lift->no);
			}
			else
			{
				// printf("DEBUG: empty Lift %d going DOWN\n", lift->no);
			}
		}

		// Check there are people waiting and lift isn't full
		if (lift->peopleinlift < MAXNOINLIFT && *waiting)
		{
			// Add person to the lift
			lift->peopleinlift++;

			// Erase the person from the waiting queue
			print_at_xy(NLIFTS * 4 + floors[lift->position].waitingtogodown + floors[lift->position].waitingtogoup, NFLOORS - lift->position, " ");

			// One less person waiting
			(*waiting)--;

			// TODO:CHECK THIS

			//---			// Set which lift the passenger should enter
			// Critical, requires protection, as explained in lecture 15
			current_lift = lift;
			// printf("DEBUG: Person entering lift %d at floor %d (%d people now in lift)\n",
			//  lift->no, lift->position, lift->peopleinlift);

			// Release floor mutex before Sleep()
			semaphore_signal(&floor_lock);

			// Wait for person to get into lift
			Sleep(GETINSPEED);

			//---			// Signal passenger to enter
			semaphore_signal(s);
		}
		else
		{
			// Release floor lock
			semaphore_signal(&floor_lock);
			// printf("DEBUG: Lift %d at floor %d is full or no more passengers waiting\n",
			// lift->no, lift->position);
			break;
		}
	}
}

// --------------------------------------------------
// Function for the Lift Threads
// --------------------------------------------------
void *lift_thread(void *p)
{
	// Local variables
	lift_info lift;
	unsigned long long int no = (unsigned long long int)p;
	int i;

	// Set up Lift
	lift.no = no;		   // Set lift number
	lift.position = 0;	   // Lift starts on ground floor
	lift.direction = UP;   // Lift starts going up
	lift.peopleinlift = 0; // Lift starts empty

	// printf("DEBUG: Initializing lift %llu\n", no);
	//  INIT sem for each floor stops
	for (i = 0; i < NFLOORS; i++)
	{
		lift.stops[i] = 0;					   // No passengers waiting
		semaphore_create(&lift.stopsem[i], 0); // Initialise semaphores
	}

	// Randomise lift
	randomise();

	// Wait for random start up time (up to a second)
	Sleep(rnd(1000));

	// printf("DEBUG: Lift %llu starting operation\n", no);

	// Loop forever
	while (true)
	{
		// Print current position of the lift
		print_at_xy(no * 4 + 1, NFLOORS - lift.position, lf);

		// Wait for a while
		Sleep(LIFTSPEED);

		// CHECK THIS
		semaphore_wait(&lift_lock); // Wait for lift lock to protect the lift data

		// Drop off passengers on this floor
		while (lift.stops[lift.position] != 0)
		{
			// One less passenger in lift
			lift.peopleinlift--;

			// One less waiting to get off at this floor
			lift.stops[lift.position]--;

			// Release lift before sleep
			semaphore_signal(&lift_lock);

			// Wait for exit lift delay
			Sleep(GETOUTSPEED);

			//---			// Signal passenger to leave lift
			semaphore_signal(&lift.stopsem[lift.position]);

			// Re-acquire lift mutex to check conditions
			semaphore_wait(&lift_lock);
			// printf("DEBUG: Lift %llu at floor %d dropping off passenger (%d people left in lift)\n",
			// no, lift.position, lift.peopleinlift);

			//  Check if that was the last passenger waiting for this floor
			if (!lift.stops[lift.position])
			{
				// Clear the "-"
				print_at_xy(no * 4 + 1 + 2, NFLOORS - lift.position, " ");
			}
		}
		// Check if lift is going up or is empty
		if (lift.direction == UP || !lift.peopleinlift)
		{
			// add locks and releases TODO?
			// semephore_signal(&lift_lock); // Release lift lock
			//  Pick up passengers waiting to go up
			get_into_lift(&lift, UP);

			// semephore_wait(&lift_lock); // Re-acquire lift lock
		}
		// Check if lift is going down or is empty
		if (lift.direction == DOWN || !lift.peopleinlift)
		{
			// SIGNAL and WAIT before getting into lift??
			//  Pick up passengers waiting to go down
			get_into_lift(&lift, DOWN);
		}

		// Erase lift from screen
		print_at_xy(no * 4 + 1, NFLOORS - lift.position, (lift.direction + 1 ? " " : lc));

		// Move lift
		lift.position += lift.direction;

		// Check if lift is at top or bottom
		if (lift.position == 0 || lift.position == NFLOORS - 1)
		{
			// Change lift direction
			lift.direction = -lift.direction;
		}
		// Release lift lock
		semaphore_signal(&lift_lock);
	}

	return NULL;
}

// --------------------------------------------------
// Function for the Person Threads
// --------------------------------------------------
void *person_thread(void *p)
{
	// Local variables
	int from = 0, to; // Start on the ground floor
	lift_info *lift;
	unsigned long long int pid = (unsigned long long int)p;

	// Randomise
	randomise();

	// printf("DEBUG: Person %llu initialized on ground floor\n", pid);

	// Stay in the building forever
	while (1)
	{
		// Work for a while
		Sleep(rnd(PEOPLESPEED));
		do
		{
			// Randomly pick another floor to go to
			to = rnd(NFLOORS);
		} while (to == from);

		// printf("DEBUG: Person %llu at floor %d wants to go to floor %d\n", pid, from, to);

		// Acquire the floor lock
		semaphore_wait(&floor_lock);

		// Check which direction the person is going (UP/DOWN)
		if (to > from)
		{
			// One more person waiting to go up
			floors[from].waitingtogoup++;

			// printf("DEBUG: Person %llu waiting to go UP from floor %d\n", pid, from);

			// Print person waiting
			print_at_xy(NLIFTS * 4 + floors[from].waitingtogoup + floors[from].waitingtogodown, NFLOORS - from, pr);

			///---			// Wait for a lift to arrive (going up)
			semaphore_signal(&floor_lock); // Release floor lock
			// This implements the semaphore wait operation from Lecture 17
			// printf("DEBUG: Person %llu waiting for UP lift at floor %d\n", pid, from);
			semaphore_wait(&floors[from].up_arrow);
			// printf("DEBUG: Person %llu signaled to enter UP lift at floor %d\n", pid, from);
		}
		else
		{
			// One more person waiting to go down
			floors[from].waitingtogodown++;
			// printf("DEBUG: Person %llu waiting to go DOWN from floor %d\n", pid, from);

			// Print person waiting
			print_at_xy(NLIFTS * 4 + floors[from].waitingtogodown + floors[from].waitingtogoup, NFLOORS - from, pr);

			///---			// Wait for a lift to arrive (going down)
			// Release floor lock
			semaphore_signal(&floor_lock);
			// This implements the semaphore wait operation from Lecture 17
			// printf("DEBUG: Person %llu waiting for DOWN lift at floor %d\n", pid, from);
			semaphore_wait(&floors[from].down_arrow);
			// printf("DEBUG: Person %llu signaled to enter DOWN lift at floor %d\n", pid, from);
		}

		// Which lift we are getting into
		///---
		lift = current_lift;
		// Print the lift number
		// printf("DEBUG: Person %llu entering lift %d at floor %d\n", pid, lift->no, from);

		// Protection
		semaphore_wait(&lift_lock); // Wait for lift lock to protect the lift data

		// Add one to passengers waiting for floor
		lift->stops[to]++;
		// printf("DEBUG: Person %llu pressed button for floor %d in lift %d\n", pid, to, lift->no);

		// Press button if we are the first
		if (lift->stops[to] == 1)
		{
			// Print light for destination
			print_at_xy(lift->no * 4 + 1 + 2, NFLOORS - to, "-");
		}

		///---		// Wait until we are at the right floor
		semaphore_signal(&lift_lock); // Release lift lock
		// printf("DEBUG: Person %llu waiting to reach floor %d in lift %d\n", pid, to, lift->no);
		semaphore_wait(&lift->stopsem[to]);
		// printf("DEBUG: Person %llu arrived at floor %d\n", pid, to);

		// Exit the lift
		from = to;
	}

	return NULL;
}

// --------------------------------------------------
//	Print the building on the screen
// --------------------------------------------------
void printbuilding(void)
{
	// Local variables
	int l, f;

	// Clear Screen
	system(clear_screen);

	//If information was printed here it would not be erased in the VS code terminal but was erased in the windows terminal
	// print_information();

	// // Print Message
	// printf("Lift Simulation - Press CTRL-Break to exit\n");
	
	

	// Print Roof
	printf("%s", tl);
	for (l = 0; l < NLIFTS - 1; l++)
	{
		printf("%s%s%s%s", hl, td, hl, td);
	}
	printf("%s%s%s%s\n", hl, td, hl, tr);

	// Print Floors and Lifts
	for (f = NFLOORS - 1; f >= 0; f--)
	{
		for (l = 0; l < NLIFTS; l++)
		{
			printf("%s%s%s ", vl, lc, vl);
			if (l == NLIFTS - 1)
			{
				printf("%s\n", vl);
			}
		}
	}

	// Print Ground
	printf("%s", bl);
	for (l = 0; l < NLIFTS - 1; l++)
	{
		printf("%s%s%s%s", hl, tu, hl, tu);
	}
	printf("%s%s%s%s\n", hl, tu, hl, br);

	//Had issues with the Lift erasing the output in the console <- if this is compiled in the windows terminal and ran from the CMD it works fine if the information is printed at the bottom of the screen
	//But VSCode terminal has issues.
	// Print my information
	print_information();

	// Print Message
	printf("Lift Simulation - Press CTRL-Break to exit\n");
	
}

// --------------------------------------------------
// Main starts the threads and then waits.
// --------------------------------------------------
int main()
{

	// Local variables
	unsigned long long int i;

	// Initialise random number generator
	randomise();

	// Initialise Building
	for (i = 0; i < NFLOORS; i++)
	{
		// Initialise Floor
		floors[i].waitingtogoup = 0;
		floors[i].waitingtogodown = 0;
		semaphore_create(&floors[i].up_arrow, 0);
		semaphore_create(&floors[i].down_arrow, 0);
	}

	// --- Initialise any other semaphores ---
	semaphore_create(&print_lock, 1); // For preventing race conditions when printing
	semaphore_create(&lift_lock, 1);  // For protecting lift data
	semaphore_create(&floor_lock, 1); // For protecting floor data
	// printf("DEBUG: Mutual exclusion semaphores initialized\n");

	// // Print Assignment
	// print_information();
	// Print Building
	printbuilding();

	// Create Lifts
	for (i = 0; i < NLIFTS; i++)
	{
		// Create Lift Thread
		create_thread(lift_thread, (void *)i);
	}

	// Create People
	for (i = 0; i < NPEOPLE; i++)
	{
		// Create Person Thread
		create_thread(person_thread, (void *)i);
	}

	// Go to sleep for 86400 seconds (one day)
	Sleep(86400000ULL);
}
