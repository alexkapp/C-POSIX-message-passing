CC = gcc
CFLAGS = -g
LIBS = -lrt
DEPS = shared_data.h 

all: master slave 

master: master.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

slave: slave.o
	$(CC) -o $@ $< $(CFLAGS)

master.o: master.c $(DEPS)
	$(CC) $(CFLAGS) -c $<

slave.o: slave.c $(DEPS)
	$(CC) $(CFLAGS) -c $< 

clean:
	/bin/rm -f *.o master slave 
