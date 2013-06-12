#include "global.h"
#include "windows.h"

struct BasicInfo
{
  struct WinMode wmode; // windows in basic info mode
};

struct BasicInfo *basic_info;

void basic_state_checking(void);
void print_basic_help(void);
void print_extra_info(void);
void print_basic_song_info(void);
void print_basic_bar(void);
