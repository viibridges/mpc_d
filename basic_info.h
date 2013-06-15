#include "global.h"
#include "windows.h"

struct BasicInfo
{
  struct WinMode wmode; // windows in basic info mode

  // if any song is played or paused
  int state;
  
  int repeat;
  int random;
  int single;
  // total number of songs in current playlist
  int total;
  // current playing id. -1 if nothing is playing
  int current; 
  int volume;
  int bit_rate;

  // information of current song
  int crt_time;
  int total_time;
  char crt_name[512];
  char format[16];
};

struct BasicInfo *basic_info;

void basic_state_checking(void);
void print_basic_help(void);
void print_extra_info(void);
void print_basic_song_info(void);
void print_basic_bar(void);

struct BasicInfo* basic_info_setup(void);
