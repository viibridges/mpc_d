#include "windows.h"
#include "utils.h"

#include "basic_info.h"
#include "songs.h"
#include "directory.h"
#include "playlists.h"
#include "visualizer.h"
#include "keyboards.h"

WINDOW*
specific_win(int win_id)
{
  WINDOW *win = wchain[win_id].win;
  werase(win);
  return win;
}

void signal_win(int id)
{
  wchain[id].redraw_signal = 1;
}

void signal_all_wins(void)
{
  int i;
  for(i = 0; i < being_mode->size; i++)
	being_mode->wins[i]->redraw_signal = 1;
}
  
void
clean_window(int id)
{
  werase(wchain[id].win);
  wrefresh(wchain[id].win);
}

void
clean_screen(void)
{
  int i;
  for(i = 0; i < being_mode->size; i++)
	{
	  werase(being_mode->wins[i]->win);
	  wrefresh(being_mode->wins[i]->win);
	}
}

void
being_mode_update(struct WinMode *wmode)
{
  being_mode = wmode;
  signal_all_wins();
}

void 
color_print(WINDOW *win, int color_scheme, const char *str)
{
  if(color_scheme > 0)
	{
	  wattron(win, my_color_pairs[color_scheme - 1]);
	  wprintw(win, "%s", str);
	  wattroff(win, my_color_pairs[color_scheme - 1]);	  
	}
  else
	wprintw(win, "%s", str);
}

void
print_list_item(WINDOW *win, int line, int color, int id,
					char *ltext, char *rtext)
{
  const int ltext_left = 6, rtext_left = 43;
  const int width = win->_maxx - 8;

  wattron(win, my_color_pairs[color - 1]);

  mvwprintw(win, line, 0, "%*c", width, ' ');

  id > 0 ? mvwprintw(win, line, 0, "%3i.", id) : 1;
  ltext ? mvwprintw(win, line, ltext_left, "%s", ltext) : 1;
  rtext ? mvwprintw(win, line, rtext_left, "%s", rtext) : 1;
  
  wattroff(win, my_color_pairs[color - 1]);
}

void popup_simple_dialog(const char *message)
{
  // first do some measurements
  int width = strlen(message) + 6;
  width = width > 70 ? 70 : width;
  int height = 5;
  int x = stdscr->_maxx / 2 - width / 2;
  int y = stdscr->_maxy / 2 - height / 2;

  WINDOW *dialog = newwin(height, width, y, x);
  wmove(dialog, 2, 3);
  wprintw(dialog, message); // hope the prompt won't be too long
  wborder(dialog, 0, 0, 0, 0, 0, 0, 0, 0);

  wrefresh(dialog);

  sleep(1); // stay awhile

  // destroy the window
  werase(dialog);
  wrefresh(dialog);
  delwin(dialog);

  signal_all_wins();
}

char* popup_input_dialog(const char *prompt)
{
  static char ret[512];
  
  // first do some measurements
  int width = stdscr->_maxx / 2;
  int height = 8;
  int x = stdscr->_maxx / 2 - width / 2;
  int y = stdscr->_maxy / 2 - height / 2;

  WINDOW *dialog = newwin(height, width, y, x);
  wmove(dialog, 2, 2);
  wprintw(dialog, prompt); // hope the prompt won't be too long
  wmove(dialog, 4, 4);
  wborder(dialog, 0, 0, 0, 0, 0, 0, 0, 0);

  wrefresh(dialog);
  
  notimeout(dialog, TRUE); // block the wgetch()
  echo();
  curs_set(1);
  
  if(wscanw(dialog, "%128[^\n]%*c", ret) == ERR)
	if(*ret)
	  ErrorAndExit("char array overflows.");
	
  noecho();
  curs_set(0);

  // destroy the window
  werase(dialog);
  wrefresh(dialog);
  delwin(dialog);

  signal_all_wins();

  return ret;
}

/* return 1(yes) or 0(no) */
int popup_confirm_dialog(const char *prompt, int dflt)
{
  int ret = dflt;
  
  // first do some measurements
  int width = stdscr->_maxx / 2;
  int height = 7;
  int x = stdscr->_maxx / 2 - width / 2;
  int y = stdscr->_maxy / 2 - height / 2;

  WINDOW *dialog = newwin(height, width, y, x);
  wmove(dialog, 2, 2);
  wprintw(dialog, prompt); // hope the prompt won't be too long
  
  wborder(dialog, 0, 0, 0, 0, 0, 0, 0, 0);

  int offset = width / 4;
  
  wmove(dialog, 4, offset);
  color_print(dialog, ret ? 2 : 0, "YES");
  wmove(dialog, 4, width - offset - 3);
  color_print(dialog, ret ? 0 : 2, "NO ");
  wrefresh(dialog);
  
  notimeout(dialog, TRUE); // block the wgetch()

  keypad(dialog, TRUE);
  int key, out = 0;
  while(!out && (key = wgetch(dialog)) != '\n')
	{
	  switch(key)
		{
		case KEY_LEFT: case KEY_RIGHT:
		case '\t':
		  ret = !ret;
		  break;
		case 'y': case 'Y':
		  ret = 1;
		  out = 1;
		  break;
		case 'n': case 'N':
		  ret = 0;
		  out = 1;
		  break;
		default:
		  break;
		}

	  wmove(dialog, 4, offset);
	  color_print(dialog, ret ? 2 : 0, "YES ");
	  wmove(dialog, 4, width - offset - 3);
	  color_print(dialog, ret ? 0 : 2, " NO ");
	  wrefresh(dialog);
	}

  // destroy the window
  werase(dialog);
  wrefresh(dialog);
  delwin(dialog);

  signal_all_wins();

  return ret;

}

void debug(const char *debug_info)
{
  WINDOW *win = specific_win(DEBUG_INFO);
  if(debug_info)
	{
	  wprintw(win, debug_info);
	  wrefresh(win);
	}
}

void debug_static(const char *debug_info)
{
  static int t = 1;
  WINDOW *win = specific_win(DEBUG_INFO);
  wprintw(win, "[%i] ", t++);
  wprintw(win, debug_info);
  wrefresh(win);
}

void debug_int(const int num)
{
  WINDOW *win = specific_win(DEBUG_INFO);
  wprintw(win, "%d", num);
  wrefresh(win);
}

/* draw border for all window, this is for debugging
   purpose */
void outline_all_windows(void)
{
  int i;
  for(i = 0; i < WIN_NUM; i++)
	{
	  werase(wchain[i].win);
	  wborder(wchain[i].win, 0, 0, 0, 0, 0, 0, 0, 0);
	  wrefresh(wchain[i].win);
	}

  getchar();
}

void
wchain_init(void)
{
  void (*redraw_func[WIN_NUM])() =
	{
	  &print_basic_song_info,    // BASIC_INFO       
	  &print_extra_info,		 // EXTRA_INFO       
	  &print_basic_bar,			 // VERBOSE_PROC_BAR 
	  &print_visualizer,		 // VISUALIZER
	  &print_basic_help,		 // HELPER
	  &songlist_simple_bar,		 // SIMPLE_PROC_BAR  
	  &songlist_up_state_bar,    // SLIST_UP_STATE_BAR
	  &songlist_redraw_screen,	 // SONGLIST
	  &songlist_down_state_bar,  // SLIST_DOWN_STATE_BAR
	  &directory_redraw_screen,  // DIRECTORY
	  &directory_display_icon,   // DIRICON
	  &directory_helper,         // DIRHELPER
	  &playlist_redraw_screen,   // PLAYLIST
	  &playlist_display_icon,    // PLAYICON
	  &playlist_helper,          // PLAYHELPER
	  &search_prompt,			 // SEARCH_INPUT
	  NULL						 // DEBUG_INFO  
	};

  void (*checking_func[WIN_NUM])() =
	{
	  &basic_state_checking,    	 // BASIC_INFO       
	  NULL,		                	 // EXTRA_INFO       
	  NULL,			            	 // VERBOSE_PROC_BAR 
	  NULL,		                	 // VISUALIZER
	  NULL,		                	 // HELPER
	  NULL,		                	 // SIMPLE_PROC_BAR  
	  NULL,                     	 // SLIST_UP_STATE_BAR
	  &songlist_update_checking,	 // SONGLIST
	  NULL,                          // SLIST_DOWN_STATE_BAR
	  &directory_update_checking,    // DIRECTORY
	  NULL,                          // DIRHICON
	  NULL,                          // DIRHELPER
	  &playlist_update_checking,     // PLAYLIST
	  NULL,                          // PLAYICON
	  NULL,                          // PLAYHELPER
	  NULL,			                 // SEARCH_INPUT
	  NULL						     // DEBUG_INFO  
	};
  
  int i;
  wchain = (struct WindowUnit*)malloc(WIN_NUM * sizeof(struct WindowUnit));
  for(i = 0; i < WIN_NUM; i++)
	{
	  wchain[i].win = newwin(0, 0, 0, 0);// we're gonna change soon
	  wchain[i].redraw_routine = redraw_func[i];
	  wchain[i].update_checking = checking_func[i];
	  wchain[i].visible = 1;
	  wchain[i].flash = 0;
	}

  // some windows need refresh frequently
  wchain[VERBOSE_PROC_BAR].flash = 1;
  wchain[SIMPLE_PROC_BAR].flash = 1;
  wchain[VISUALIZER].flash = 1;

  // and some need to hide
  wchain[HELPER].visible = 0;
  wchain[VISUALIZER].visible = 0;
  wchain[SEARCH_INPUT].visible = 0;
  
  wchain_size_update();
}

// basically sizes are what we are concerned
void
wchain_size_update(void)
{
  static int old_width = -1, old_height = -1;
  int height = stdscr->_maxy + 1, width = stdscr->_maxx + 1;

  if(old_height == height && old_width == width)
	return;
  else
	old_height = height, old_width = width;

  int wparam[WIN_NUM][4] =
	{
	  {2, width, 0, 0},             // BASIC_INFO
	  {1, width - 47, 2, 46},       // EXTRA_INFO
	  {1, 42, 2, 0},				// VERBOSE_PROC_BAR
	  {1, 72, 4, 0},				// VISUALIZER
	  {9, width, 5, 0},				// HELPER
	  {1, 29, 4, 43},				// SIMPLE_PROC_BAR
	  {1, 42, 4, 0},				// SLIST_UP_STATE_BAR
	  {height - 8, 73, 5, 0},	    // SONGLIST
	  {1, width, height - 3, 0},	// SLIST_DOWN_STATE_BAR
	  {height - 8, 36, 6, 41},	    // DIRECTORY
	  {6, 20, 4, 10},               // DIRICON
	  {10, 29, 12, 2},              // DIRHELPER
	  {8, 36, 6, 41},	            // PLAYLIST
	  {5, 12, 16, 52},	            // PLAYICON
	  {13, 26, 4, 2},               // PLAYHELPER
	  {1, width, height - 1, 0},	// SEARCH_INPUT
	  {1, width, height - 2, 0}		// DEBUG_INFO       
	}; 

  int i;
  for(i = 0; i < WIN_NUM; i++)
	{
	  wresize(wchain[i].win, wparam[i][0], wparam[i][1]);
	  mvwin(wchain[i].win, wparam[i][2], wparam[i][3]);
	}
}

void wchain_free(void)
{
  free(wchain);
}

void color_init(void)
{
  if(has_colors() == FALSE)
	{
	  endwin();
	  fprintf(stderr, "your terminal does not support color\n");
	  exit(1);
	}
  start_color();
  init_pair(1, COLOR_YELLOW, COLOR_BLACK);
  my_color_pairs[0] = COLOR_PAIR(1) | A_BOLD;
  init_pair(2, COLOR_WHITE, COLOR_BLUE);
  my_color_pairs[1] = COLOR_PAIR(2) | A_BOLD;
  init_pair(3, COLOR_MAGENTA, COLOR_BLACK);  
  my_color_pairs[2] = COLOR_PAIR(3) | A_BOLD;
  init_pair(4, COLOR_RED, COLOR_BLACK);  
  my_color_pairs[3] = COLOR_PAIR(4) | A_BOLD;
  init_pair(5, COLOR_GREEN, COLOR_BLACK);  
  my_color_pairs[4] = COLOR_PAIR(5) | A_BOLD;
  init_pair(6, COLOR_CYAN, COLOR_BLACK);  
  my_color_pairs[5] = COLOR_PAIR(6) | A_BOLD;
  init_pair(7, COLOR_BLUE, COLOR_BLACK);
  my_color_pairs[6] = COLOR_PAIR(7) | A_BOLD;
  init_pair(8, 0, COLOR_BLACK);
  my_color_pairs[7] = COLOR_PAIR(8) | A_BOLD;
  init_pair(9, COLOR_YELLOW, COLOR_WHITE);
  my_color_pairs[8] = COLOR_PAIR(9) | A_BOLD;
  use_default_colors();
}

void
screen_redraw(void)
{
  int i;
  struct WindowUnit **wunit = being_mode->wins;
  for(i = 0; i < being_mode->size; i++)
	{
	  if(wunit[i]->visible && wunit[i]->redraw_signal
		 && wunit[i]->redraw_routine)
		{
		  wunit[i]->redraw_routine();
		  wrefresh(wunit[i]->win);
		}
	  wunit[i]->redraw_signal = wunit[i]->flash;
	}
}

void
screen_update_checking(void)
{
  int i;
  struct WindowUnit **wunit = being_mode->wins;
  for(i = 0; i < being_mode->size; i++)
	{
	  if(wunit[i]->visible && wunit[i]->update_checking)
		wunit[i]->update_checking();
	}
}
