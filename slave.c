/*******************************
 * Alex Kappelmann             *
 * User Process Program        *
 *******************************/

#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include "shared_data.h"
#include <sys/types.h>
#include <sys/msg.h>

#define BILLION 1000000000L

void sig_handler(int signal);
void getTime(char *timestr);
int id;

int main (int argc, char **argv) {
  
	signal(SIGINT, sig_handler); 
	srand(getpid());
	
	logicalClock_t *log_clock;
	mymsg_t msgbuf;
	id = atoi(argv[1]);	
  	int shmid, msqid;
  	int len, quit = 0;
	unsigned long curtime, termtime;
		
	key_t key = ftok(".",'K'); //Same key as master process

	/* Get and attatch to shared memory segment */
	if ((shmid = shmget(key, sizeof(logicalClock_t *), 0)) < 0) {
        	fprintf(stderr, "shget:child failed to get the shared memory segment\n");
       	 	exit(1);
    	}
	
	if ((log_clock = shmat(shmid, NULL, SHM_RDONLY)) == (void *) -1) {
        	fprintf(stderr, "shmat:child failed attatching to shared memory\n");
        	exit(1);
    	}
	
	/* Get the Message Queue */
	if ((msqid = msgget(1234, 0666)) < 0) {
    		fprintf(stderr, "smget:child %d failed to get msg queue\n", id);
	  	exit(0);
	}

	/* Calculate random amount of time before sending terminate msg */
	curtime = (log_clock->sec * BILLION) + log_clock->nsec;
	termtime = curtime + (50000000 + rand() % 100000000);

	while (!quit) {
		/*** Entry Section ***/
		msgrcv(msqid, &msgbuf, 80, mutex, 0); 
				
		/*** Critical Section ***/
		curtime = (log_clock->sec * BILLION) +  log_clock->nsec;
		unsigned long exit_time = curtime + (1+ rand() % 5000000);

		/* run crit. section until its time to exit */
		while (curtime < exit_time) {
			
			curtime = (log_clock->sec * BILLION) +  log_clock->nsec;
			/* check if its time to terminate */
			if (curtime >= termtime) {
			
				char str[12];
				sprintf(str, "%.9lf", (double)termtime/BILLION);
				strcpy (msgbuf.mtext, str);
				msgbuf.mtype = term;
				len = strlen(msgbuf.mtext) +1;

				msgsnd(msqid, &msgbuf, len, 0);
				quit = 1;

			 	/* Process blocks until the master sends a "done" message
			 	* to ensure that this process' pid is the current `msqid_ds lspid' 
 		 		* when the master is processing the "term" message
 				*/
				msgrcv(msqid, &msgbuf, 1, done, 0);
				break;	
			}
		}	
    		/*** Exit Section ***/
		msgbuf.mtype = mutex;
		strcpy(msgbuf.mtext, "\0");
		len = 1;
		msgsnd(msqid, &msgbuf, len, IPC_NOWAIT);
	}

	/* Detatch from shared memory and exit. */
	if (shmdt(log_clock) == -1) {
		fprintf(stderr, "Process %d failed to detatch from shared memory\n", id);
		exit(-1);
	}
	else {
		fprintf(stderr,"Process %d successfully detatched from shared memory\n", id);
		exit(0);
	}
}

void sig_handler(int signal) {

	if (signal == SIGINT) {

		fprintf(stderr, "Child %d: Terminating: Recieved SIGINT.\n", id );
		exit(SIGINT);
	}
}
