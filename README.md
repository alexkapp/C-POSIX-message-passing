# C-POSIX-message-passing
Uses the C POSIX library for shared memory and message passing to enforce mutual exclusive execution of a critical section between multiple concurrent processes <br>
- Operating System: GNU/Linux 
- Kernel: Linux 
- Version: 3.10.0-514.2.2.el7.x86_64 (builder@kbuilder.dev.centos.org) (gcc version 4.8.5 20150623 (Red Hat 4.8.5-11) (GCC) ) #1 SMP Tue Dec 6 23:06:41 UTC 2016 
<br>

# master.c
### The main program, which serves as the master process
This program starts off by first allocating shared memory for a logical clock that only it can increment, but can 
be read by child processes. Then, it forks off the appropriate number of child processes (default:5) that will compete
to enter their critical section and then enters a loop. Every iteration of the loop, it increments the the logical clock (simulating time passing in the system) and processes any messages sent to it by the child processes. Upon receiving a 
message from one of its children, it outputs the contents of the message and current time to a file (default: test.out) 
and forks off another child until the max number of child processes to spawn is hit (default: 50). This process continues
until 2 seconds have passed on the logical clock, the maximum duration of real-time has passed (default: 5.0 seconds), 
or all child processes have terminated. Once all child processes have terminated, it deallocates the shared memory and exits.
<br><br>Optional Command line Arguments:
- -h: prints options
- -s [int]: Number of initial slaves to spawn (default: 5)
- -n [int]: Max number of total slaves spawned (default 50)
- -l [filename]: The log file containing the program's output (default: test.out)
- -t [double]: Maximum time the program will be allowed to run (default: 5.0) 

# slave.c
### The program ran by child processes, forked by the master.
This process starts by reading the system clock generated by master and generates a random duration that represents 
how long this child should run. It then loops continually over a critical section of code enforced through message passing 
(msgsnd and msgrcv) between slave processes. In the critical section, the slave process reads the master clock until a random 
amount of time has passed. If while in the critical section it sees that its duration to run is up, it sends a message to master that it is going to terminate. Once the child knows master received the message, it terminates itself. The message sent to the master consists of the the child’s pid and the time that it decided to terminate on.
