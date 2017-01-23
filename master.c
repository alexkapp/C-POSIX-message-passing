/********************************
 * Alex Kappelmann              *
 * Master Process Program       *
 ********************************/

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include "shared_data.h"
#include <string.h>

#define BILLION 1000000000L
#define PERM (S_IRUSR | S_IWUSR)

static volatile int quitflag = 0;

void sig_handler(int sig);
static int startTimer(double sec);
int spawnchild(int);

int main(int argc, char **argv) {

	srand(getpid());

	/* Install signal handlers */
	signal(SIGINT, sig_handler);
	signal(SIGALRM, sig_handler);

 	logicalClock_t *log_clock;
	pid_t wpid;
  	mymsg_t msgbuf;
	struct msqid_ds msgstat;
	FILE *fd;

	int shmid, msqid;
	int numproc = 10, slavecount = 0, numrunning = 0, maxslaves = 50;
	int i, c, msgsize;
	double curtime; 
  	double run_time = 5.0; //maximum time the program will run in realtime 
	char outfile[20] = "test.out";
    
	/* Parse command line options */
	while ( (c = getopt(argc, argv, "hs:n:l:t:" )) != -1 ) {

		switch(c) {

			case 'h':
				fprintf(stderr, "\nUsage: %s: [-h] [-s number] [-n number] [-l filename] [-t number] \n"
						"    -s number: Number of initial slaves to spawn (default: 5)\n"
						"    -n number: Max number of total slaves spawned (default 50)\n"
						"    -l filename: The log file used (default: test.out)\n"
						"    -t number: The time in seconds when the master will terminate itself (default: 5) \n", argv[0]+2);
				return 0;
				break;
			case 's':
				numproc = atoi(optarg);
				break;
			case 'n':
				maxslaves = atoi(optarg);
				break;
			case 'l':
				sprintf(outfile, "%s", optarg);
				break;
			case 't':
				run_time = (double) atof(optarg);
				break;
			default:
				break;
		}
	}

	/* Open file */	
	if ( (fd =fopen(outfile, "w")) == NULL ) {
		fprintf(stderr, "Unable to open file\n");
		return -1;
	} 	

	/* Allocate and attatch to shared memory segment */
	if ((shmid = shmget(ftok(".", 'K'), sizeof(logicalClock_t), IPC_CREAT | PERM)) < 0) {
		fprintf(stderr, "shmget: master failed to allocate shared memory segment\n");
    		exit(1);
	}

   	 if ((log_clock = (logicalClock_t *) shmat(shmid, NULL, 0)) == (void *) -1) {

   		fprintf(stderr, "shmat: master failed to attach to shared memory segment\n");
		if (shmctl(shmid, IPC_RMID, NULL) == -1)
			fprintf(stderr, "master failed to remove shared memory segment\n");
		else
			fprintf(stderr, "master successfully deallocated shared memory\n");
       		exit(1);
    	}

	/* Create Message Queue */
	if ( (msqid = msgget(1234, PERM | IPC_CREAT)) < 0 ) {
		fprintf(stderr, "master failed to create message queue\n");
		return msqid;
	}

	/* Initialze logicalClock_t variables */
	log_clock->sec = 0;
	log_clock->nsec = 0;

  	//creates and starts a real-time timer of the max program run time
	startTimer(run_time);

	/* Spawn Initial child processes */
	for (i = 1; (i <= numproc) && (quitflag != 1);i++) 
		if ( (spawnchild(i)) == -1 ) 
			fprintf(stderr, "Master: fork error\n");
		else {	
			slavecount++;
			numrunning++;
		}

	/* Unblock first process */
	msgbuf.mtype = mutex;
	strcpy(msgbuf.mtext, "\0");
	if ( (msgsnd(msqid, &msgbuf, 1, IPC_NOWAIT)) < 0 ) {
		fprintf(stderr, "master failed sending message");
	}

	/* Update logical clock and check for messages */
	int offset = getpid();
	while (!quitflag && numrunning > 0) {

		log_clock->nsec += 1000 + rand() % 9000;
		if (log_clock->nsec >= BILLION) {
			log_clock->sec++;
			log_clock->nsec -= BILLION;
		}
		
		if (log_clock->sec == 2)
			break; 
		
    		//check for any messages in the queue
		if ( (msgsize = msgrcv(msqid, &msgbuf, 80, term, IPC_NOWAIT)) > 0) {
			
			msgctl(msqid, IPC_STAT, &msgstat);
			int chpid = msgstat.msg_lspid - offset;
			curtime =  (double) log_clock->sec + ( (double) log_clock->nsec / BILLION);  
			
			fprintf(fd, "Master: Child %3d is terminating at mytime %.9lf because it reached %s\n", 
					chpid, curtime, msgbuf.mtext);

			/* Send "done" message to tell the current process who's in the Crit Sect to continue.
 	  		 * This ensures that the child's pid who sent the "term" message stays as 
  			 * current 'msqid_ds msg_lspid'
  			*/
			msgbuf.mtype = done;
			strcpy(msgbuf.mtext, "\0");
			msgsnd(msqid, &msgbuf, 1, IPC_NOWAIT);	
			
			/* Wait for child process to exit before spawning another child */
			int status;	
			wait(&status);	
			numrunning--;
		
			if (slavecount < maxslaves)
				if ( (spawnchild(slavecount+1)) == -1 ) 	
					fprintf(stderr, "Master: fork error\n");
				else {
					slavecount++;
					numrunning++;
				}
		}
	}
	fprintf(stderr, "MASTER: finished at %.9lf\n", (double) (log_clock->sec *BILLION + log_clock->nsec) / BILLION);  
	if (numrunning > 0) 
		kill(-getpid(), SIGINT);

	/* Wait for all processes to exit */
	int stat = 0;
	while ( (wpid = wait(&stat)) > 0 );	

	
	/*All child processes have exited: deallocate shared memory*/
	fclose(fd);
	int error;
	
	if ( msgctl(msqid, IPC_RMID, NULL) < 0 ) {
		fprintf(stderr, "master failed to detatch from shared memory\n");
		error = errno;
	}
	else fprintf(stderr, "Master Successfully removed mailbox\n");
	if ( shmdt(log_clock) == -1 ) {
		fprintf(stderr, "master failed to detatch from shared memory\n");
		error = errno;
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
		fprintf(stderr, "master failed to remove shared memory segment\n");
		error = errno;
	}

	if (!error) {
		fprintf(stderr, "master successfully deallocated shared memory\n");
		return 0;
	}
	errno = error;
	return error;
}

/* spawns a child process */
int spawnchild(int id) {

	int childpid = fork();
	if (childpid == -1) {
		return -1;
	}
	else if (childpid == 0) {
	
		signal(SIGINT,SIG_DFL);	
		char argID[5];
		sprintf(argID, "%d\0", id);
		
		if ((execl("./slave", "./slave", argID, 0)) == -1) {
			fprintf(stderr, "child %d failed to execute user prog.\n",id);
			exit(-1);
		}
	}
	return 0;
}

static int startTimer(double sec) {

	timer_t timerid;
	struct itimerspec value;

	if (timer_create(CLOCK_REALTIME, NULL, &timerid) == -1)
		return -1;
	value.it_interval.tv_sec = (long)sec;
	value.it_interval.tv_nsec = (sec - value.it_interval.tv_sec)*BILLION;
	if (value.it_interval.tv_nsec >= BILLION) {
		value.it_interval.tv_sec++;
		value.it_interval.tv_nsec -= BILLION;
	}
	value.it_value = value.it_interval;
	return timer_settime(timerid,0,&value,NULL);
}

void sig_handler(int signl) {

	//Sends this signal to child processes incase they missed it.
	if (signl == SIGINT) {
		quitflag = 1;
		signal(SIGINT, SIG_IGN); //Ignore signal sent to self to continue with clean-up 
		fprintf(stderr, "Master (process 0): recieved SIGINT: Waiting for child processes to terminate.\n");
		while (kill(-getpid(), SIGINT) != 0);
	}

	//The master recieves this signal from the timer.
	if (signl == SIGALRM) {
		quitflag = 1;
		fprintf(stderr, "Master (process 0): Recieved SIGALRM:  Signalling child processes to terminate.\n", (long)getpid());
		signal(SIGINT,SIG_IGN); //Ignore signal sent to self to continue with clean-up.
		kill(-getpid(), SIGINT);
	}
}
