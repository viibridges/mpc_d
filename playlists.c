#include "playlists.h"
#include "utils.h"

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
playlist_helper(void)
{
  WINDOW *win = specific_win(TAPEHELPER);

  wprintw(win,  "\n\
  Key Options:\n\
  ----------------------\n\
    [k] Move Cursor Up\n\
    [j] Move Cursor Down\n\
    [a] Append Current\n\
    [r] Rename Playlist");
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
	
	while((entity = mpd_recv_entity(conn)) != NULL && i < 128)
	  {
		enum mpd_entity_type type = mpd_entity_get_type(entity);
		if(type != MPD_ENTITY_TYPE_PLAYLIST)
		  continue;
		
		plist = mpd_entity_get_playlist(entity);
		strncpy(playlist->tapename[i],
				mpd_playlist_get_path(plist), 512);

		mpd_entity_free(entity);

		i++;
	  }

	playlist->length = i;
}

void playlist_rename(void)
{
  char *to =
	popup_input_dialog("Enter a New Name Here:");

  int choice =
	popup_confirm_dialog("Do You Really Mean That?");

  if(!choice) // action canceled
	return;

  const char *from = playlist->tapename[playlist->cursor - 1];
	
  mpd_run_rename(conn, from, to);

  playlist->update_signal = 1;
}

void playlist_append(void)
{
  const char *name = playlist->tapename[playlist->cursor - 1];

  mpd_run_load(conn, name);
}
