#include "keyboards.h"

#include "basic_info.h"
#include "songs.h"
#include "directory.h"
#include "playlists.h"
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
	  switch_to_songlist_menu();
	  break;
	case '1':
	  switch_to_main_menu();
	  break;
	case '2':
	  switch_to_songlist_menu();
	  break;
	case '3':
	  switch_to_directory_menu();
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

void songlist_keymap_template(int key)
{
  // filter those different with the template
  switch(key)
	{
	case 'v': break; // key be masked

	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  songlist_scroll_down_line();
	  break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  songlist_scroll_up_line();break;
	case '\n':
	  songlist_play_current();break;
	case 'b':
	  songlist_scroll_up_page();break;
	case ' ':
	  songlist_scroll_down_page();break;
	case 'i':;
	case 12: ; // ctrl-l
	case 'l':  // cursor goto current playing place
	  songlist_scroll_to_current();
	  break;
	case 'g':  // cursor goto the beginning
	  songlist_scroll_to(1);
	  break;
	case 'G':  // cursor goto the end
	  songlist_scroll_to(songlist->length);
	  break;
	case 'c':  // cursor goto the center
	  songlist_scroll_to(songlist->length / 2);
	  break;
	case 'D':
	  songlist_delete_song();
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
directory_keymap_template(int key)
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
	  append_to_songlist();break;
	  
	case 14: ; // ctrl-n
	case KEY_DOWN:;
	case 'j':
	  directory_scroll_down_line();break;
	case 16: ; // ctrl-p
	case KEY_UP:;
	case 'k':
	  directory_scroll_up_line();break;
	case 'b':
	  directory_scroll_up_page();break;
	case ' ':
	  directory_scroll_down_page();break;
	case 'g':  // cursor goto the beginning
	  directory_scroll_to(1);
	  break;
	case 'G':  // cursor goto the end
	  directory_scroll_to(directory->length);
	  break;
	case 'c':  // cursor goto the center
	  directory_scroll_to(directory->length / 2);
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

// for picking the song from the searchmode
// corporate with searchmode_keymap()
void
searchmode_picking_keymap(void)
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
	  songlist_play_current();
	  turnoff_search_mode();
	  break;

	case 127:
	  // push the backspace back to delete the
	  // last character and force to update the
	  // searchmode by reemerge the keyboard event.
	  // and the rest works are the same with a
	  // normal "continue searching" invoked by '/'
	  ungetch(127);
	case '/':  // search on
	  // switch to the basic keymap for searching
	  // we only change being_mode->listen_keyboard instead of
	  // wchain[SONGLIST]->key_map, that's doesn't matter,
	  // the latter was fixed and any changes could lead to
	  // unpredictable behaviors.
	  being_mode->listen_keyboard = &searchmode_keymap; 
	  songlist->picking_mode = 0;

	  songlist_cursor_hide();
	  signal_win(SONGLIST);
	  signal_win(SEARCH_INPUT);
	  break;

	case '\\':
	  turnoff_search_mode(); break;
	default:
	  songlist_keymap_template(key);
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
songlist_keymap(void)
{
  int key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  songlist_keymap_template(key);

  signal_all_wins();  
}

void
directory_keymap(void)
{
  int key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  directory_keymap_template(key);

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
searchmode_keymap(void)
{
  int i, key = getch();

  if(key != ERR)
	interval_level = 1;
  else
	return;

  i = strlen(songlist->key);
  
  switch(key)
	{
	case '\n':;
	case '\t':
	  if(i == 0)
		{
		  turnoff_search_mode();
		  break;
		}

	  being_mode->listen_keyboard = &searchmode_picking_keymap;
	  songlist->picking_mode = 1;

	  songlist_scroll_to(1);

	  songlist->update_signal = 1;	  
	  signal_win(SONGLIST);
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
	  
	  if(!isascii(songlist->key[i - 1]))
		songlist->key[i - 3] = '\0';
	  else
		songlist->key[i - 1] = '\0';
	  
	  songlist->update_signal = 1;
	  signal_win(SONGLIST);
	  signal_win(SEARCH_INPUT);
	  break;
	default:
	  if(!isascii(key))
		{
		  songlist->key[i++] = (char)key;
		  songlist->key[i++] = getch();
		  songlist->key[i++] = getch();
		}
	  else
		songlist->key[i++] = (char)key;
	  songlist->key[i] = '\0';
	  
	  songlist->update_signal = 1;
	  signal_win(SEARCH_INPUT);
	  signal_win(SONGLIST);
	}
}

