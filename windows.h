#include "global.h"

#ifndef F98IQJFNASKFJSAODI
#define F98IQJFNASKFJSAODI

struct WindowUnit *wchain; // array stores all subwindows
struct WinMode *being_mode; // pointer to current used window set
int my_color_pairs[9];

enum window_id
  {
	BASIC_INFO,              // title, status, play mode ...
	EXTRA_INFO,				 // volume meter, search tag ...
	VERBOSE_PROC_BAR,		 // time bar for basic mode
	VISUALIZER,				 // visaulizer: vu meter
	HELPER,					 // help page
	SIMPLE_PROC_BAR,		 // time bar for songlist mode
	SLIST_UP_STATE_BAR,		 // implies scroll up
	SONGLIST,				 // songlist
	SLIST_DOWN_STATE_BAR,	 // implies scroll down and copy right
	DIRECTORY,				 // window list item in current directory
	DIRICON,                 // directory icon
	DIRHELPER,               // directory instruction 
	PLAYLIST,                // window list all playlists
	PLAYICON,                // icon window for playlist
	PLAYHELPER,              // playlist instruction 
	SEARCH_INPUT,			 // search prompt area
	DEBUG_INFO,				 // for debug perpuse only
	WIN_NUM                  // number of windows
  };

struct WindowUnit
{
  int visible;
  // if visible and 1 means this window redraw alway
  // every time, its redraw_signal always be 1;
  int flash;
  int redraw_signal;
  
  WINDOW *win;

  // each window has its own redraw routine and check routine
  void (*redraw_routine)(void);
  void (*update_checking)(void);
};

struct WinMode
{
  int size;
  struct WindowUnit **wins;

  void (*listen_keyboard)(void);
};


/*************************************
 **     WINDOW  CONTROLER           **
 *************************************/
WINDOW* specific_win(int win_id);
void signal_win(int id);
void signal_all_wins(void);
void clean_window(int id);
void clean_screen(void);
void being_mode_update(struct WinMode *wmode);
void color_print(WINDOW *win, int color_scheme, const char *str);
void print_list_item(WINDOW *win, int line, int color, int id,
					 char *ltext, char *rtext);

void popup_simple_dialog(const char *message);
char* popup_input_dialog(const char *prompt);
int popup_confirm_dialog(const char *prompt, int dflt);

void wchain_init(void);
void wchain_size_update(void);
void wchain_free(void);

void color_init(void);

void debug(const char *debug_info);
void debug_static(const char *debug_info);
void debug_int(const int num);
void outline_all_windows(void);

void screen_update_checking(void);
void screen_redraw(void);

#endif
