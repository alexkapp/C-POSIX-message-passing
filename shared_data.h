/*************************** 
 * Alex Kappelmann         *
 * Header File             *
****************************/

#ifndef shared_data_h
#define shared_data_h

typedef struct {

	int sec;
	long int nsec;
} logicalClock_t;

typedef struct {

	long mtype;
	char mtext[80];
} mymsg_t;

enum msg {first, mutex, term, done};

#endif /* shared_data_h */
