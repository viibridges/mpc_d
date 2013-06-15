CC = gcc
CLIBS = -lm -lmpdclient -lncursesw
CFLAGS = -std=gnu99 -Wall -g

BIN = mpc_d
PREFIX = /usr/local/bin
OBJECTS = basic_info.o commands.o directory.o initial.o keyboards.o songs.o playlists.o utils.o visualizer.o windows.o

#main: $(HEAD) $(SOURCE)
#	$(CC) $(SOURCE) -o $(BIN) $(CLIBS) $(CFLAGS)

mpc_d: main.c $(OBJECTS)
	$(CC) $(OBJECTS) main.c -o $(BIN) $(CLIBS) $(CFLAGS)

initial.o: initial.c initial.h 
	$(CC) -c initial.c -o initial.o $(CLIBS) $(CFLAGS)	

keyboards.o: keyboards.c keyboards.h 
	$(CC) -c keyboards.c -o keyboards.o $(CLIBS) $(CFLAGS)	

commands.o: commands.c commands.h 
	$(CC) -c commands.c -o commands.o $(CLIBS) $(CFLAGS)	

visualizer.o: visualizer.c visualizer.h 
	$(CC) -c visualizer.c -o visualizer.o $(CLIBS) $(CFLAGS)	

playlists.o: playlists.c playlists.h 
	$(CC) -c playlists.c -o playlists.o $(CLIBS) $(CFLAGS)	

directory.o: directory.c directory.h 
	$(CC) -c directory.c -o directory.o $(CLIBS) $(CFLAGS)	

songs.o: songs.c songs.h 
	$(CC) -c songs.c -o songs.o $(CLIBS) $(CFLAGS)	

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

run:
	@./$(BIN)

install: $(BIN)
	mv $(BIN) $(PREFIX)
