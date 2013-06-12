CC = gcc
CLIBS = -lm -lmpdclient -lncursesw
CFLAGS = -std=gnu99 -Wall -g

BIN = mpc
OBJECTS = basic_info.o commands.o dirlist.o initial.o keyboards.o playlist.o searchlist.o tapelist.o utils.o visualizer.o windows.o

SOURCE = basic_info.c commands.c dirlist.c initial.c keyboards.c main.c playlist.c searchlist.c tapelist.c utils.c visualizer.c windows.c
HEAD = basic_info.h commands.h dirlist.h global.h initial.h keyboards.h playlist.h searchlist.h tapelist.h utils.h visualizer.h windows.h



#main: $(HEAD) $(SOURCE)
#	$(CC) $(SOURCE) -o $(BIN) $(CLIBS) $(CFLAGS)

mpc: main.c $(OBJECTS)
	$(CC) $(OBJECTS) main.c -o $(BIN) $(CLIBS) $(CFLAGS)

initial.o: initial.c initial.h 
	$(CC) -c initial.c -o initial.o $(CLIBS) $(CFLAGS)	

keyboards.o: keyboards.c keyboards.h 
	$(CC) -c keyboards.c -o keyboards.o $(CLIBS) $(CFLAGS)	

commands.o: commands.c commands.h 
	$(CC) -c commands.c -o commands.o $(CLIBS) $(CFLAGS)	

visualizer.o: visualizer.c visualizer.h 
	$(CC) -c visualizer.c -o visualizer.o $(CLIBS) $(CFLAGS)	

tapelist.o: tapelist.c tapelist.h 
	$(CC) -c tapelist.c -o tapelist.o $(CLIBS) $(CFLAGS)	

dirlist.o: dirlist.c dirlist.h 
	$(CC) -c dirlist.c -o dirlist.o $(CLIBS) $(CFLAGS)	

searchlist.o: searchlist.c searchlist.h 
	$(CC) -c searchlist.c -o searchlist.o $(CLIBS) $(CFLAGS)	

playlist.o: playlist.c playlist.h 
	$(CC) -c playlist.c -o playlist.o $(CLIBS) $(CFLAGS)	

basic_info.o: basic_info.c basic_info.h
	$(CC) -c basic_info.c -o basic_info.o $(CLIBS) $(CFLAGS)	

windows.o: windows.c windows.h
	$(CC) -c windows.c -o windows.o $(CLIBS) $(CFLAGS)

utils.o: utils.c utils.h
	$(CC) -c utils.c -o utils.o $(CLIBS) $(CFLAGS)

# dynamic.o: windows.o utils.o dynamic.c dynamic.h
# 	$(CC) windows.o utils.o dynamic.c -o dynamic.o $(CLIBS) $(CFLAGS)

clean:
	rm *.o -f $(BIN)
