#include "utils.h"

/*************************************
 **    SYSTEM  AND  MISCELLANY      **
 *************************************/
void
printErrorAndExit(struct mpd_connection *conn)
{
	const char *message;

	assert(mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS);

	message = mpd_connection_get_error_message(conn);

	endwin();
	
	fprintf(stderr, "Error: %s\n", message);
	
	exit(EXIT_FAILURE);
}

/* the principle is that: if the keyboard events
 * occur frequently, then adjust the update rate
 * higher, if the keyboard is just idle, keep it
 * low. */
void
smart_sleep(void)
{
  static int us = INTERVAL_MAX_UNIT;

  if(interval_level)
	{
	  us = INTERVAL_MIN_UNIT +
		(interval_level - 1) * 10000;
	  interval_level = 0;
	}
  else
	us = us < INTERVAL_MAX_UNIT ? us + INTERVAL_INCREMENT : us;

  usleep(us);
}

void my_finishCommand(struct mpd_connection *conn) {
  if (!mpd_response_finish(conn))
	printErrorAndExit(conn);
}

struct mpd_status *
getStatus(struct mpd_connection *conn) {
  struct mpd_status *ret = mpd_run_status(conn);
  if (ret == NULL)
	printErrorAndExit(conn);

  return ret;
}

struct mpd_status*
init_mpd_status(struct mpd_connection *conn)
{
  struct mpd_status *status;

  if (!mpd_command_list_begin(conn, true) ||
	  !mpd_send_status(conn) ||
	  !mpd_send_current_song(conn) ||
	  !mpd_command_list_end(conn))
	printErrorAndExit(conn);

  status = mpd_recv_status(conn);
  if (status == NULL)
	printErrorAndExit(conn);

  return status;
}

int
is_dir_exist(char *path)
{
  struct stat s;

  if (!stat(path, &s) && (s.st_mode & S_IFDIR))
	return 1;
  else
	return 0;
}

int
is_path_exist(char *path)
{
  struct stat s;

  if (!stat(path, &s) && (s.st_mode & (S_IFDIR | S_IFREG | S_IFLNK)))
	return 1;
  else
	return 0;
}

const char *
get_song_format(const struct mpd_song *song)
{
  const char *profix;
  // normally the profix is a song's format;
  profix = strrchr(mpd_song_get_uri(song), (int)'.');
  if(profix == NULL) return "unknown";
  else return profix + 1;
}

const char *
get_song_tag(const struct mpd_song *song, enum mpd_tag_type type)
{
  const char *song_tag, *uri;
  song_tag = mpd_song_get_tag(song, type, 0);

  if(song_tag == NULL)
	{
	  if(type == MPD_TAG_TITLE)
		{
		  uri = strrchr(mpd_song_get_uri(song), (int)'/');
		  if(uri == NULL) return mpd_song_get_uri(song);
		  else return uri + 1;
		}
	  else
		return "Unknown";
	}
  else
	return song_tag;
}

char *is_substring_ignorecase(const char *main, char *sub)
{
  char lower_main[64], lower_sub[64];
  int i;

  for(i = 0; main[i] && i < 64; i++)
  	lower_main[i] = isalpha(main[i]) ?
  	  (islower(main[i]) ? main[i] : (char)tolower(main[i])) : main[i];
  lower_main[i] = '\0';

  for(i = 0; sub[i] && i < 64; i++)
  	lower_sub[i] = isalpha(sub[i]) ?
  	  (islower(sub[i]) ? sub[i] : (char)tolower(sub[i])) : sub[i];
  lower_sub[i] = '\0';

  return strstr(lower_main, lower_sub);
}

void
pretty_copy(char *string, const char * tag, int size, int width)
{
  static int i, unic, asci;

  for(i = unic = asci = 0; tag[i] != '\0' && i < size - 1; i++)
	{
	  if(isascii((int)tag[i]))
		asci ++;
	  else
		unic ++;
	  string[i] = tag[i];
	}

  unic /= 3;

  if(width > 0)
	{
	  /* if the string too long, we interupt it
		 and profix ... to it */
	  if(width < 2*unic + asci)
		{
		  strncpy(string + width - 3, "...", 4);
		  return;
		}
	}

  string[i] = '\0';
}

