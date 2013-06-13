#include "keyboards.h"

#include "basic_info.h"
#include "playlist.h"
#include "searchlist.h"
#include "dirlist.h"
#include "tapelist.h"
#include "visualizer.h"
#include "commands.h"

void fundamental_keymap_template(int key)
{
  switch(key)
	{
	case '+': ;
	case '=': ;
	case '0': 
	  cmd_volup(); break;
	case KEY_RIGHT:
	  cmd_forward(); break;
	case '-': ;
	case '9': 
	  cmd_voldown(); break;
	case KEY_LEFT:
	  cmd_backward(); break;
	case 'b':
	  cmd_Playback(); break;
	case 'n':
	  cmd_Next(); break;
	case 'p':
	  cmd_Prev(); break;
	case 't': ;
	case ' ':
	  cmd_Toggle(); break;
	case 'r':
	  cmd_Random(); break;
	case 's':
	  cmd_Single(); break;
	case 'O':;
	case 'o':;
	case 'R':
	  cmd_Repeat(); break;
	case 'L': // redraw screen
	  clean_screen();
	  break;
	case '/':
	  turnon_search_mode(); break;
	case 'S':
	  change_searching_scope(); break;
	case '\t':
	  switch_to_playlist_menu();
	  break;
	case '1':
	  switch_to_main_menu();
	  break;
	case '2':
	  switch_to_playlist_menu();
	  break;
	case '3':
	  switch_to_dirlist_menu();
	  break;
	case '4':
	  switch_to_tapelist_menu();
	  break;
	case 'v':
	  toggle_visualizer();
	  break;
	case 27: ;
	case 'e': ;
	case 'q':
	  quit_signal = 1; break;
	default:;
	}
}

void playlist_keymap_template(int key)
{
  // filter those different with the template
  switch(key)
	{
	case 'v': break; // key be masked

	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  playlist_scroll_down_line();
	  break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  playlist_scroll_up_line();break;
	case '\n':
	  playlist_play_current();break;
	case 'b':
	  playlist_scroll_up_page();break;
	case ' ':
	  playlist_scroll_down_page();break;
	case 'i':;
	case 12: ; // ctrl-l
	case 'l':  // cursor goto current playing place
	  playlist_scroll_to_current();
	  break;
	case 'g':  // cursor goto the beginning
	  playlist_scroll_to(1);
	  break;
	case 'G':  // cursor goto the end
	  playlist_scroll_to(playlist->length);
	  break;
	case 'c':  // cursor goto the center
	  playlist_scroll_to(playlist->length / 2);
	  break;
	case 'D':
	  playlist_delete_song();
	  break;
	case '\t':
	  switch_to_main_menu();
	  break;
	case 'U':
	case 'K':
	  song_move_up();
	  break;
	case 'J':
	  song_move_down();
	  break;
	case 'm':
	  toggle_select();
	  break;
	  
	default:
	  fundamental_keymap_template(key);
	}
}

void
dirlist_keymap_template(int key)
{
  // filter those different with the template
  switch(key)
	{
	case 'U':;
	case 'J':;
	case 'K':;
	case 'm':;
	case 'v': break; // key be masked

	case '\n':
	  enter_selected_dir();break;
	case 127:
	  exit_current_dir();break;
	case 'a':
	  append_to_playlist();break;
	  
	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  dirlist_scroll_down_line();break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  dirlist_scroll_up_line();break;
	case 'b':
	  dirlist_scroll_up_page();break;
	case ' ':
	  dirlist_scroll_down_page();break;
	case 'g':  // cursor goto the beginning
	  dirlist_scroll_to(1);
	  break;
	case 'G':  // cursor goto the end
	  dirlist_scroll_to(dirlist->length);
	  break;
	case 'c':  // cursor goto the center
	  dirlist_scroll_to(dirlist->length / 2);
	  break;
	case '\t':
	  switch_to_main_menu();
	  break;
	  
	default:
	  fundamental_keymap_template(key);
	}
}

void
tapelist_keymap_template(int key)
{
  // filter those different with the template
  switch(key)
	{
	case 'U':;
	case 'J':;
	case 'K':;
	case 'm':;
	case 'v': break; // key be masked

	case 'r':
	  tapelist_rename();
	  break;
	case 'a':
	  tapelist_append();
	  break;
	  
	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  tapelist_scroll_down_line();
	  break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  tapelist_scroll_up_line();break;
	case 'b':
	  tapelist_scroll_up_page();break;
	case ' ':
	  tapelist_scroll_down_page();break;
	case 'g':  // cursor goto the beginning
	  tapelist_scroll_to(1);
	  break;
	case 'G':  // cursor goto the end
	  tapelist_scroll_to(tapelist->length);
	  break;
	case 'c':  // cursor goto the center
	  tapelist_scroll_to(tapelist->length / 2);
	  break;
	case '\t':
	  switch_to_main_menu();
	  break;
	  
	default:
	  fundamental_keymap_template(key);
	}
}

// for picking the song from the searchlist
// corporate with searchlist_keymap()
void
searchlist_picking_keymap(void)
{
  int key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  switch(key)
	{
	case 'U':;
	case 'J':;
	case 'K':;
	case 'm':;
	case 'i':;
	case '\t':;
	case 'l': break; // these keys are masked
	  
	case '\n':
	  playlist_play_current();
	  turnoff_search_mode();
	  break;

	case 127:
	  // push the backspace back to delete the
	  // last character and force to update the
	  // searchlist by reemerge the keyboard event.
	  // and the rest works are the same with a
	  // normal "continue searching" invoked by '/'
	  ungetch(127);
	case '/':  // search on
	  // switch to the basic keymap for searching
	  // we only change being_mode->listen_keyboard instead of
	  // wchain[SEARCHLIST]->key_map, that's doesn't matter,
	  // the latter was fixed and any changes could lead to
	  // unpredictable behaviors.
	  being_mode->listen_keyboard = &searchlist_keymap; 
	  searchlist->picking_mode = 0;

	  playlist_cursor_hide();
	  signal_win(SEARCHLIST);
	  signal_win(SEARCH_INPUT);
	  break;

	case '\\':
	  turnoff_search_mode(); break;
	default:
	  playlist_keymap_template(key);
	}

  signal_all_wins();
}

void
basic_keymap(void)
{
  int key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  // get the key, let the template do the rest
  fundamental_keymap_template(key);

  signal_all_wins();
}

void
playlist_keymap(void)
{
  int key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  playlist_keymap_template(key);

  signal_all_wins();  
}

void
dirlist_keymap(void)
{
  int key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  dirlist_keymap_template(key);

  signal_all_wins();  
}

void
tapelist_keymap(void)
{
  int key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  tapelist_keymap_template(key);

  signal_all_wins();  
}

// for getting the keyword
void
searchlist_keymap(void)
{
  int i, key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  i = strlen(searchlist->key);
  
  switch(key)
	{
	case '\n':;
	case '\t':
	  if(i == 0)
		{
		  turnoff_search_mode();
		  break;
		}

	  being_mode->listen_keyboard = &searchlist_picking_keymap;
	  searchlist->picking_mode = 1;

	  playlist_scroll_to(1);

	  searchlist->update_signal = 1;	  
	  signal_win(SEARCHLIST);
	  signal_win(SEARCH_INPUT);
	  break;
	case '\\':;
	case 27:
	  turnoff_search_mode();
	  break;
	case 127: // backspace is hitted
	  if(i == 0)
		{
		  turnoff_search_mode();
		  break;
		}
	  
	  if(!isascii(searchlist->key[i - 1]))
		searchlist->key[i - 3] = '\0';
	  else
		searchlist->key[i - 1] = '\0';
	  
	  searchlist->update_signal = 1;
	  signal_win(SEARCHLIST);
	  signal_win(SEARCH_INPUT);
	  break;
	default:
	  if(!isascii(key))
		{
		  searchlist->key[i++] = (char)key;
		  searchlist->key[i++] = getch();
		  searchlist->key[i++] = getch();
		}
	  else
		searchlist->key[i++] = (char)key;
	  searchlist->key[i] = '\0';
	  
	  searchlist->update_signal = 1;
	  signal_win(SEARCH_INPUT);
	  signal_win(SEARCHLIST);
	}
}

