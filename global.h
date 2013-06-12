#include <mpd/client.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <ncursesw/ncurses.h>
#include <locale.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/stat.h>

#define SEEK_UNIT 3
#define VOLUME_UNIT 3
#define INTERVAL_MIN_UNIT 20000
#define INTERVAL_MAX_UNIT 200000
#define INTERVAL_INCREMENT 800
#define MAX_PLAYLIST_STORE_LENGTH 700
#define BUFFER_SIZE 32

int quit_signal;
// 1 for the minimum interval, 0 for dynamic (increase),
// the larger the longer, the scope is between
// INTERVAL_MIN_UNIT and  INTERVAL_MAX_UNIT
int interval_level;

struct mpd_connection *conn;
