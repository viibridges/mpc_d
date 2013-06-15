#include "playlists.h"
#include "utils.h"
#include "keyboards.h"

void
playlist_redraw_screen(void)
{
  int line = 0, i, height = wchain[PLAYLIST].win->_maxy + 1;

  WINDOW *win = specific_win(PLAYLIST);  

  char *filename;
  for(i = playlist->begin - 1; i < playlist->begin
		+ height - 1 && i < playlist->length; i++)
	{
	  filename = playlist->tapename[i];

	  if(i + 1 == playlist->cursor)
		print_list_item(win, line++, 2, i + 1, filename, NULL);
	  else
		print_list_item(win, line++, 0, i + 1, filename, NULL);
	}
}

void
playlist_display_icon(void)
{
  WINDOW *win = specific_win(PLAYICON);
/*   color_print(win, 1, "\ */
/* ╔══╗ ♫\n\ */
/* ║██║ ♪♪\n\ */
/* ║██║♫♪\n\ */
/* ║ ◎♫♪♫\n\ */
/* ╚══╝"); */
  color_print(win, 1, "\
 ♫\n\
♪♪ ╔══╗  _\n\
 ♪♫║██║*´ )\n\
 ♫♪♫◎ ║.•´\n\
   ╚══╝¸.•o");
}

void
playlist_helper(void)
{
  WINDOW *win = specific_win(PLAYHELPER);

  color_print(win, 4, "\
Playlists:");
  wprintw(win,  "\n\n\
    <k> Move Cursor Up...\n\
    <j> Move Cursor Down.\n");
  color_print(win, 6, "\n\
    [l] Load to Current..\n\
    [s] Save from Current\n\
    [d] Delelet Playlist.\n\
    [r] Rename Playlist..\n\
    [C] Cover Playlist...\n\
    [R] Replace Current..");
  wprintw(win, "\n\n\
    [c] Clear Current....");
}

void playlist_update_checking(void)
{
  if(playlist->update_signal)
	{
	  playlist_update();
	  playlist->update_signal = 0;
	  signal_all_wins();
	}
}

void
playlist_update(void)
{
	struct mpd_entity *entity;
	const struct mpd_playlist *plist;

	int i = 0;

	if (!mpd_send_list_meta(conn, ""))
	  return printErrorAndExit(conn);
	
	while((entity = mpd_recv_entity(conn)) != NULL)
	  {
		if(i >= 128) break;

		enum mpd_entity_type type = mpd_entity_get_type(entity);
		if(type != MPD_ENTITY_TYPE_PLAYLIST)
		  {
			mpd_entity_free(entity);
			continue;
		  }
		
		plist = mpd_entity_get_playlist(entity);
		strncpy(playlist->tapename[i],
				mpd_playlist_get_path(plist), 512);

		mpd_entity_free(entity);

		i++;
	  }

	playlist->length = i;
}

static int
is_playlist_name_conflict(const char* name)
{
  int i;
  
  for(i = 0; i < playlist->length; i++)
	{
	  if(strcmp(playlist->tapename[i], name) == 0)
		return 1;
	}

  return 0;
}

void playlist_rename(void)
{
  char *to =
	popup_input_dialog("New Name Here:");

  if(is_playlist_name_conflict(to))
	{
	  char message[512];
	  snprintf(message, sizeof(message),
			   "\"%s\" Conflict With Another.", to);
	  popup_simple_dialog(message);
	  
	  return;
	}

  if(!*to) // empty string
	return;
	
  int choice =
	popup_confirm_dialog("Renaming Confirm:", 1);

  if(!choice) // action canceled
	return;

  const char *from = playlist->tapename[playlist->cursor - 1];

  mpd_run_rename(conn, from, to);

  playlist->update_signal = 1;
}

void playlist_load(void)
{
  const char *name = playlist->tapename[playlist->cursor - 1];

  mpd_run_load(conn, name);

  char message[512];
  snprintf(message, sizeof(message), "\"%s\" Has Been Appended.", name);
  popup_simple_dialog(message);
}

void playlist_save(void)
{
  char *name =
	popup_input_dialog("New Name Here:");

  if(is_playlist_name_conflict(name))
	{
	  char message[512];
	  snprintf(message, sizeof(message),
			   "\"%s\" Conflict With Another.", name);
	  popup_simple_dialog(message);
	  
	  return;
	}
	
  int choice =
	popup_confirm_dialog("Saving Confirm:", 1);

  if(!choice) // action canceled
	return;

  mpd_run_save(conn, name);

  playlist->update_signal = 1;  
}

void playlist_cover(void)
{
  const char *name = playlist->tapename[playlist->cursor - 1];

  char message[512];

  int c =
	popup_confirm_dialog("Covering Confirm:", 0);

  if(!c) return;
  
  mpd_run_rm(conn, name);
  mpd_run_save(conn, name);

  snprintf(message, sizeof(message), "\"%s\" Has Been Covered.", name);
  popup_simple_dialog(message);
}

void playlist_delete(void)
{
  const char *name = playlist->tapename[playlist->cursor - 1];

  char message[512];

  int c =
	popup_confirm_dialog("Deletion Confirm:", 0);

  if(!c) return;
  
  mpd_run_rm(conn, name);

  snprintf(message, sizeof(message), "\"%s\" Has Been Deleted.", name);
  popup_simple_dialog(message);

  playlist->update_signal = 1;  
}

void playlist_replace(void)
{
  int choice =
	popup_confirm_dialog("Replacing Confirm:", 1);

  if(!choice) // action canceled
	return;

  const char *name = playlist->tapename[playlist->cursor - 1];

  mpd_run_clear(conn);
  mpd_run_load(conn, name);

  char message[512];
  snprintf(message, sizeof(message), "Replace With \"%s\".", name);
  popup_simple_dialog(message);
}

struct Playlist *playlist_setup(void)
{
  struct Playlist *plist =
	(struct Playlist*) malloc(sizeof(struct Playlist));

  plist->update_signal = 0;
  plist->begin = 1;
  plist->length = 0;
  plist->cursor = 1;

  // window mode setup
  plist->wmode.size = 6;
  plist->wmode.wins = (struct WindowUnit**)
	malloc(plist->wmode.size * sizeof(struct WindowUnit*));
  plist->wmode.wins[0] = &wchain[PLAYHELPER];
  plist->wmode.wins[1] = &wchain[EXTRA_INFO];  
  plist->wmode.wins[2] = &wchain[PLAYLIST];  
  plist->wmode.wins[3] = &wchain[SIMPLE_PROC_BAR];
  plist->wmode.wins[4] = &wchain[BASIC_INFO];
  plist->wmode.wins[5] = &wchain[PLAYICON];
  plist->wmode.listen_keyboard = &playlist_keymap;

  return plist;
}

void playlist_free(struct Playlist *plist)
{
  free(plist->wmode.wins);
  free(plist);
}

void
playlist_scroll_to(int line)
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  playlist->cursor = 0;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, line);
}

void
playlist_scroll_down_line(void)
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, +1);
}

void
playlist_scroll_up_line(void)
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, -1);
}

void
playlist_scroll_up_page(void)
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, -15);  
}

void
playlist_scroll_down_page(void)
{
  int height = wchain[PLAYLIST].win->_maxy + 1;
  scroll_line_shift_style(&playlist->cursor, &playlist->begin,
						  playlist->length, height, +15);  
}

