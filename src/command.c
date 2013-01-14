/* music player command (mpc)
 * Copyright (C) 2003-2008 Warren Dukes <warren.dukes@gmail.com>,
				Eric Wong <normalperson@yhbt.net>,
				Daniel Brown <danb@cs.utexas.edu>
 * Copyright (C) 2008-2010 Max Kellermann <max@duempel.org>
 * Project homepage: http://musicpd.org

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "command.h"
#include "charset.h"
#include "options.h"
#include "util.h"
#include "search.h"
#include "status.h"
#include "gcc.h"

#include <mpd/client.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <curses.h>
#include <locale.h>

#define DIE(...) do { fprintf(stderr, __VA_ARGS__); return -1; } while(0)

#define SIMPLE_CMD(funcname, libmpdclient_funcname, ret) \
int funcname(mpd_unused int argc, mpd_unused char **argv, \
	     struct mpd_connection *conn) { \
        libmpdclient_funcname(conn); \
        my_finishCommand(conn); \
        return ret; \
}

#define SIMPLE_ONEARG_CMD(funcname, libmpdclient_funcname, ret) \
int funcname (mpd_unused int argc, char **argv, struct mpd_connection *conn) { \
	if (!libmpdclient_funcname(conn, charset_to_utf8(argv[0]))) \
		printErrorAndExit(conn); \
        return ret; \
}

static void my_finishCommand(struct mpd_connection *conn) {
	if (!mpd_response_finish(conn))
		printErrorAndExit(conn);
}

SIMPLE_CMD(cmd_next, mpd_run_next, 1)
SIMPLE_CMD(cmd_prev, mpd_run_previous, 1)
SIMPLE_CMD(cmd_stop, mpd_run_stop, 1)
SIMPLE_CMD(cmd_clear, mpd_run_clear, 1)
SIMPLE_CMD(cmd_shuffle, mpd_run_shuffle, 1)

#if LIBMPDCLIENT_CHECK_VERSION(2,4,0)
SIMPLE_CMD(cmd_clearerror, mpd_run_clearerror, 1)
#endif

SIMPLE_ONEARG_CMD(cmd_save, mpd_run_save, 0)
SIMPLE_ONEARG_CMD(cmd_rm, mpd_run_rm, 0)

static struct mpd_status *
getStatus(struct mpd_connection *conn) {
	struct mpd_status *ret = mpd_run_status(conn);
	if (ret == NULL)
		printErrorAndExit(conn);

	return ret;
}

int cmd_add (int argc, char ** argv, struct mpd_connection *conn )
{
	int i;

	if (!mpd_command_list_begin(conn, false))
		printErrorAndExit(conn);

	for(i=0;i<argc;i++) {
		if (options.verbosity >= V_VERBOSE)
			printf("adding: %s\n", argv[i]);
		mpd_send_add(conn, charset_to_utf8(argv[i]));
	}

	if (!mpd_command_list_end(conn))
		printErrorAndExit(conn);

	if (!mpd_response_finish(conn)) {
#if LIBMPDCLIENT_CHECK_VERSION(2,4,0)
		if (mpd_connection_get_error(conn) == MPD_ERROR_SERVER) {
			/* check which of the arguments has failed */
			unsigned location =
				mpd_connection_get_server_error_location(conn);
			if (location < (unsigned)argc) {
				/* we've got a valid location from the
				   server */
				const char *message =
					mpd_connection_get_error_message(conn);
				message = charset_from_utf8(message);
				fprintf(stderr, "error adding %s: %s\n",
					argv[location], message);
				exit(EXIT_FAILURE);
			}
		}
#endif

		printErrorAndExit(conn);
	}

	return 0;
}

int
cmd_crop(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	struct mpd_status *status = getStatus( conn );
	int length = mpd_status_get_queue_length(status) - 1;

	if (length < 0) {

		mpd_status_free(status);
		DIE( "A playlist longer than 1 song in length is required to crop.\n" );

	} else if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
		   mpd_status_get_state(status) == MPD_STATE_PAUSE) {
		if (!mpd_command_list_begin(conn, false))
			printErrorAndExit(conn);

		while( length >= 0 )
		{
			if (length != mpd_status_get_song_pos(status)) {
				mpd_send_delete(conn, length);
			}
			length--;
		}

		mpd_status_free(status);

		if (!mpd_command_list_end(conn) || !mpd_response_finish(conn))
			printErrorAndExit(conn);

		return ( 0 );

	} else {

		mpd_status_free(status);
		DIE( "You need to be playing to crop the playlist\n" );

	}
}

/**
 * Returns the id of the current song, but only if it is really
 * playing (playing or paused).
 */
static int
get_active_song(const struct mpd_status *status)
{
	return mpd_status_get_state(status) == MPD_STATE_PLAY ||
		mpd_status_get_state(status) == MPD_STATE_PAUSE
		? mpd_status_get_song_id(status)
		: -1;
}

/**
 * Wait until the song changes or until playback is started/stopped.
 */
static void
wait_current(struct mpd_connection *c)
{
	if (mpd_connection_cmp_server_version(c, 0, 14, 0) < 0)
		fprintf(stderr, "warning: MPD 0.14 required for this command\n");

	struct mpd_status *status = mpd_run_status(c);
	if (status == NULL)
		printErrorAndExit(c);

	const int old_song = get_active_song(status);
	mpd_status_free(status);

	int new_song;
	do {
		enum mpd_idle idle = mpd_run_idle_mask(c, MPD_IDLE_PLAYER);
		if (idle == 0)
			printErrorAndExit(c);

		status = mpd_run_status(c);
		if (status == NULL)
			printErrorAndExit(c);

		new_song = get_active_song(status);
		mpd_status_free(status);
	} while (new_song == old_song);
}

int cmd_current(mpd_unused int argc, mpd_unused char ** argv, struct mpd_connection *conn)
{
	if (options.wait)
		wait_current(conn);

	struct mpd_status *status;

	if (!mpd_command_list_begin(conn, true) ||
	    !mpd_send_status(conn) ||
	    !mpd_send_current_song(conn) ||
	    !mpd_command_list_end(conn))
		printErrorAndExit(conn);

	status = mpd_recv_status(conn);
	if (status == NULL)
		printErrorAndExit(conn);

	if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	    mpd_status_get_state(status) == MPD_STATE_PAUSE) {
		struct mpd_song *song;

		if (!mpd_response_next(conn))
			printErrorAndExit(conn);

		song = mpd_recv_song(conn);
		if (song != NULL) {
			pretty_print_song(song);
			printf("\n");

			mpd_song_free(song);
		}

		if (!mpd_response_finish(conn))
			printErrorAndExit(conn);
	}
	mpd_status_free(status);

	return 0;
}

/** list song around, the default span is 3;
 ** this function is adapted from cmd_current() */
int cmd_list_around(mpd_unused int argc, mpd_unused char ** argv, struct mpd_connection *conn)
{
  if (options.wait)
	wait_current(conn);

  struct mpd_status *status;

  if (!mpd_command_list_begin(conn, true) ||
	  !mpd_send_status(conn) ||
	  !mpd_send_current_song(conn) ||
	  !mpd_command_list_end(conn))
	printErrorAndExit(conn);

  status = mpd_recv_status(conn);
  if (status == NULL)
	printErrorAndExit(conn);

  if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	  mpd_status_get_state(status) == MPD_STATE_PAUSE) {
	struct mpd_song *song;

	if (!mpd_response_next(conn))
	  printErrorAndExit(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
	  int i = 1, current_id, span;
	  
	  current_id = mpd_song_get_id(song) + 1;
	  span = argc > 0 ? atoi(argv[0]) : 3;

	  mpd_song_free(song);
	  my_finishCommand(conn);	  

	  if(!mpd_send_list_queue_meta(conn))
		printErrorAndExit(conn);
	  
	  while((song = mpd_recv_song(conn)) != NULL)
		{
		  if(i >= current_id - span && i <= current_id + span)
			{
			  printf("%d",i);
			  if( i == current_id )
				printf("+  ");
			  else
				printf(".  ");
			  pretty_print_song(song);
			  printf("\n");
			}
		  
		  mpd_song_free(song);
		  i++;
		}
	}

	my_finishCommand(conn);
  }
  mpd_status_free(status);

  return 0;
}

int cmd_playback(mpd_unused int argc, mpd_unused char ** argv, struct mpd_connection *conn)
{
  int ret;
  char *sargv[1], s[8];
  
  if(argc == 0)
	snprintf(s, 7, "0");
  else
	snprintf(s, 7, "%d%%", atoi(argv[0]));

  sargv[0] = s;
  ret = cmd_seek(1, sargv, conn);

  return ret;
}

/** list song around, the default span is 3;
 ** this function is adapted from cmd_current() */
int cmd_illustrate(mpd_unused int argc, mpd_unused char ** argv, struct mpd_connection *conn)
{
  if (options.wait)
	wait_current(conn);

  struct mpd_song *song;
  int i = 1, current_id, span = 3, playlist_len;
  
  struct mpd_status *status = getStatus(conn);
  playlist_len = mpd_status_get_queue_length(status);
  
  if(argc == 0)
	current_id = playlist_len / 2;
  else
	{
	  current_id = atoi(argv[0]);
	  if(current_id < 1)
		current_id = 1;
	  else if(current_id > playlist_len)
		current_id = playlist_len;
	  if(argc > 1)
		span = atoi(argv[1]);
	}

  mpd_status_free(status);

  if(!mpd_send_list_queue_meta(conn))
	printErrorAndExit(conn);
	  
  while((song = mpd_recv_song(conn)) != NULL)
	{
	  if(i >= current_id - span && i <= current_id + span)
		{
		  printf("%d",i);
		  if( i == current_id )
			printf("+  ");
		  else
			printf(".  ");
		  pretty_print_song(song);
		  printf("\n");
		}
		  
	  mpd_song_free(song);
	  i++;
	}

  my_finishCommand(conn);

  return 0;
}

int cmd_del ( int argc, char ** argv, struct mpd_connection *conn )
{
	int i,j;
	char * s;
	char * t;
	char * t2;
	int range[2];
	int songsDeleted = 0;
	int plLength = 0;
	char * songsToDel;
	struct mpd_status *status;

	status = getStatus(conn);

	plLength = mpd_status_get_queue_length(status);

	songsToDel = malloc(plLength);
	memset(songsToDel,0,plLength);

	for(i=0;i<argc;i++) {
		if(argv[i][0]=='#') s = &(argv[i][1]);
		else s = argv[i];

		range[0] = strtol(s,&t,10);

		/* If argument is 0 current song and we're not stopped */
		if(range[0] == 0 && strlen(s) == 1 && \
			(mpd_status_get_state(status) == MPD_STATE_PLAY ||
			 mpd_status_get_state(status) == MPD_STATE_PAUSE))
			range[0] = mpd_status_get_song_pos(status) + 1;

		if(s==t)
			DIE("error parsing song numbers from: %s\n",argv[i]);
		else if(*t=='-') {
			range[1] = strtol(t+1,&t2,10);
			if(t+1==t2 || *t2!='\0')
				DIE("error parsing range from: %s\n",argv[i]);
		}
		else if(*t==')' || *t=='\0') range[1] = range[0];
		else
			DIE("error parsing song numbers from: %s\n",argv[i]);

		if(range[0]<=0 || range[1]<=0) {
			if (range[0]==range[1])
				DIE("song number must be positive: %i\n",range[0]);
			else
				DIE("song numbers must be positive: %i to %i\n",range[0],range[1]);
		}

		if(range[1]<range[0])
			DIE("song range must be from low to high: %i to %i\n",range[0],range[1]);

		if(range[1]>plLength)
			DIE("song number does not exist: %i\n",range[1]);

		for(j=range[0];j<=range[1];j++) songsToDel[j-1] = 1;
	}

	if (!mpd_command_list_begin(conn, false))
		printErrorAndExit(conn);

	for(i=0;i<plLength;i++) {
		if(songsToDel[i]) {
			mpd_send_delete(conn, i - songsDeleted);
			songsDeleted++;
		}
	}

	mpd_status_free(status);
	free(songsToDel);

	if (!mpd_command_list_end(conn) || !mpd_response_finish(conn))
		printErrorAndExit(conn);

	return 0;
}

int
cmd_toggle(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	struct mpd_status *status;
	status = getStatus(conn);

	if (mpd_status_get_state(status) == MPD_STATE_PLAY) {
		cmd_pause(0, NULL, conn);
	} else {
		cmd_play(0, NULL, conn);
	}
	return 1;
}

int
cmd_outputs(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	struct mpd_output *output;

	mpd_send_outputs(conn);

	while ((output = mpd_recv_output(conn)) != NULL) {
		/* We increment by 1 to make it natural to the user  */
		int id = mpd_output_get_id(output) + 1;
		const char *name = mpd_output_get_name(output);

		if (mpd_output_get_enabled(output)) {
			printf("Output %i (%s) is enabled\n", id, name);
		} else {
			printf("Output %i (%s) is disabled\n", id, name);
		}

		mpd_output_free(output);
	}
	mpd_response_finish(conn);
	return( 0 );
}

int
cmd_enable(mpd_unused int argc, char **argv, struct mpd_connection *conn)
{
	int arg;

        if( ! parse_int( argv[0], &arg ) || arg <= 0 ) {
		DIE( "Not a positive integer\n" );
	}

	mpd_run_enable_output(conn, arg - 1);
	return cmd_outputs(0, NULL, conn);
}

int
cmd_disable(mpd_unused int argc, char **argv, struct mpd_connection *conn)
{
	int arg;

        if( ! parse_int( argv[0], &arg ) || arg <= 0 ) {
		DIE( "Not a positive integer\n" );
	}

	mpd_run_disable_output(conn, arg - 1);
	return cmd_outputs(0, NULL, conn);
}

int cmd_play ( int argc, char ** argv, struct mpd_connection *conn )
{
	int song;
	int i;

	if(0==argc) song = -1;
	else {
		struct mpd_status *status;

		for(i=0;i<argc-1;i++)
			printf("skipping: %s\n",argv[i]);

                if(!parse_songnum(argv[i], &song))
			DIE("error parsing song numbers from: %s\n",argv[i]);

		song--;

		/* This is necessary, otherwise mpc will output the wrong playlist number */
		status = getStatus(conn);
		i = mpd_status_get_queue_length(status);
		mpd_status_free(status);
		if(song >= i)
			DIE("song number greater than playlist length.\n");
	}

	if (song >= 0)
		mpd_run_play_pos(conn, song);
	else
		mpd_run_play(conn);

	return 1;
}

int
cmd_seek(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	struct mpd_status *status;
	char * arg = argv[0];
	char * test;
	
	int seekchange;
	int total_secs;
	int seekto;
        int rel = 0;

	status = getStatus(conn);

	if (mpd_status_get_state(status) == MPD_STATE_STOP)
		DIE("not currently playing\n");

	/* Detect +/- if exists point to the next char */
        if(*arg == '+') rel = 1;
        else if(*arg == '-') rel = -1;

	if(rel != 0) arg++;

	/* If seeking by percent */
	if( arg[strlen(arg)-1] == '%' ) {

		double perc;

		/* Remove the % */
		arg[ strlen(arg) - 1 ] = '\0';

		/* percent seek, strtod is needed for percent with decimals */
		perc = strtod(arg,&test);

		if(( *test!='\0' ) || (!rel && (perc<0 || perc>100)) || (rel && perc>abs(100)))
			DIE("\"%s\" is not an number between 0 and 100\n",arg);

		seekchange = perc * mpd_status_get_total_time(status) / 100 + 0.5;

	} else { /* If seeking by absolute seek time */

		if( strchr( arg, ':' )) {
			char * sec_ptr;
			char * min_ptr;
			char * hr_ptr;

			int hr = 0;
			int min = 0;
			int sec = 0;

			/* Take the seconds off the end of arg */
			sec_ptr = strrchr( arg, ':' );

			/* Remove ':' and move the pointer one byte up */
			* sec_ptr = '\0';
			++sec_ptr;

			/* If hour is in the argument, else just point to the arg */
			if(( min_ptr = strrchr( arg, ':' ))) {

				/* Remove ':' and move the pointer one byte up */
				* min_ptr = '\0';
				++min_ptr;

				/* If the argument still exists, it's the hour  */
				if( arg != NULL ) {
					hr_ptr = arg;
					hr = strtol( hr_ptr, &test, 10 );

					if( *test != '\0' || ( ! rel && hr < 0 ))
						DIE("\"%s\" is not a positive number\n", sec_ptr);
				}
			} else {
				min_ptr = arg;
			}

			/* Change the pointers to a integer  */
			sec = strtol( sec_ptr, &test, 10 );

			if( *test != '\0' || ( ! rel && sec < 0 ))
				DIE("\"%s\" is not a positive number\n", sec_ptr);

			min = strtol( min_ptr, &test, 10 );

			if( *test != '\0' || ( ! rel && min < 0 ))
				DIE("\"%s\" is not a positive number\n", min_ptr);

			/* If mins exist, check secs. If hrs exist, check mins  */
			if( min && strlen(sec_ptr) != 2 )
				DIE("\"%s\" is not two digits\n", sec_ptr);
			else if( hr && strlen(min_ptr) != 2 )
				DIE("\"%s\" is not two digits\n", min_ptr);

			/* Finally, make sure they're not above 60 if higher unit exists */
			if( min && sec > 60 )
				DIE("\"%s\" is greater than 60\n", sec_ptr);
			else if( hr && min > 60 )
				DIE("\"%s\" is greater than 60\n", min_ptr);

			total_secs = ( hr * 3600 ) + ( min * 60 ) + sec;

		} else {

			/* absolute seek (in seconds) */
			total_secs = strtol( arg, &test, 10 ); /* get the # of seconds */

			if( *test != '\0' || ( ! rel && total_secs < 0 ))
				DIE("\"%s\" is not a positive number\n", arg);
		}
		seekchange = total_secs;
	}

	/* This detects +/- and is necessary due to the parsing of HH:MM:SS numbers*/
	if(rel == 1) {
		seekto = mpd_status_get_elapsed_time(status) + seekchange;
	} else if (rel == -1) {
		seekto = mpd_status_get_elapsed_time(status) - seekchange;
	} else {
		seekto = seekchange;
	}

	if (seekto > (int)mpd_status_get_total_time(status))
		DIE("Seek amount would seek past the end of the song\n");

	if (!mpd_run_seek_id(conn, mpd_status_get_song_id(status), seekto))
		printErrorAndExit(conn);

	mpd_status_free(status);

	return 1;
}

int
cmd_move(mpd_unused int argc, char **argv, struct mpd_connection *conn)
{
	int from;
	int to;

	if(!parse_int(argv[0], &from) || from<=0)
		DIE("\"%s\" is not a positive integer\n",argv[0]);

	if(!parse_int(argv[1], &to) || to<=0)
		DIE("\"%s\" is not a positive integer\n",argv[1]);

	/* users type in 1-based numbers, mpd uses 0-based */
	from--;
	to--;

	mpd_run_move(conn, from, to);

	return 0;
}

int
cmd_playlist(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	struct mpd_song *song;

	if (!mpd_send_list_queue_meta(conn))
		printErrorAndExit(conn);

	int i = 1;
	while ((song = mpd_recv_song(conn)) != NULL) {
	  printf("%d. ", i++);
	  pretty_print_song(song);
	  mpd_song_free(song);
	  printf("\n");
	}

	my_finishCommand(conn);

	return 0;
}

#include <ctype.h>

static
int is_substring_ignorecase(const char *main, char *sub)
{
  char lower_main[64], lower_sub[64];
  const char *pt;
  int i;

  pt = main, i = 0;
  for(i = 0; main[i] && i < 64; i++)
	lower_main[i] = isalpha(main[i]) ?
	  (islower(main[i]) ? main[i] : (char)tolower(main[i])) : main[i];
  lower_main[i] = '\0';

  for(i = 0; sub[i] && i < 64; i++)
	lower_sub[i] = isalpha(sub[i]) ?
	  (islower(sub[i]) ? sub[i] : (char)tolower(sub[i])) : sub[i];
  lower_sub[i] = '\0';

  return (int)strstr(lower_main, lower_sub);
}

int
cmd_search_general(mpd_unused int argc, mpd_unused char **argv,
				   struct mpd_connection *conn, enum mpd_tag_type search_type)
{
	struct mpd_song *song;
	int i, j = 0;
	const char *song_item;
	
	if (!mpd_send_list_queue_meta(conn))
		printErrorAndExit(conn);

	for(i = 0; i < argc-1; i++)
	  printf("skip '%s'\n", argv[i]);

	while ((song = mpd_recv_song(conn)) != NULL)
	  {
		j++;
		song_item = mpd_song_get_tag(song, search_type, 0);
		
		if(song_item)
		  if(is_substring_ignorecase(song_item, argv[i]))
			{
			  printf("%d. ", j);
			  pretty_print_song(song);
			  printf("\n");
			}

		mpd_song_free(song);
	  }

	my_finishCommand(conn);

	return 0;
}

int cmd_search_title ( int argc, char ** argv, struct mpd_connection *conn )
{
  return cmd_search_general(argc, argv, conn, MPD_TAG_TITLE);
}

int cmd_search_artist ( int argc, char ** argv, struct mpd_connection *conn )
{
  return cmd_search_general(argc, argv, conn, MPD_TAG_ARTIST);
}

int cmd_search_album ( int argc, char ** argv, struct mpd_connection *conn )
{
  return cmd_search_general(argc, argv, conn, MPD_TAG_ALBUM);
}

int cmd_listall ( int argc, char ** argv, struct mpd_connection *conn )
{
	const char * listall = "";
	int i=0;

	if (argc > 0)
		listall = charset_to_utf8(argv[i]);

	do {
		if (options.custom_format) {
			if (!mpd_send_list_all_meta(conn, listall))
				printErrorAndExit(conn);

			print_entity_list(conn, MPD_ENTITY_TYPE_UNKNOWN);
		} else {
			if (!mpd_send_list_all(conn, listall))
				printErrorAndExit(conn);

			print_filenames(conn);
		}

		my_finishCommand(conn);

	} while (++i < argc && (listall = charset_to_utf8(argv[i])) != NULL);

	return 0;
}

int cmd_update ( int argc, char ** argv, struct mpd_connection *conn)
{
	const char * update = "";
	int i = 0;
	unsigned id = 0;

	if (!mpd_command_list_begin(conn, false))
		printErrorAndExit(conn);

	if(argc > 0) update = charset_to_utf8(argv[i]);

	do {
		mpd_send_update(conn, update);
	} while (++i < argc && (update = charset_to_utf8(argv[i])) != NULL);

	if (!mpd_command_list_end(conn))
		printErrorAndExit(conn);

	/* obtain the last "update id" response */

	while (true) {
		unsigned next_id = mpd_recv_update_id(conn);
		if (next_id == 0)
			break;
		id = next_id;
	}

	if (!mpd_response_finish(conn))
		printErrorAndExit(conn);

	while (options.wait) {
		/* idle until an update finishes */
		enum mpd_idle idle = mpd_run_idle_mask(conn, MPD_IDLE_UPDATE);
		struct mpd_status *status;
		unsigned current_id;

		if (idle == 0)
			printErrorAndExit(conn);

		/* determine the current "update id" */

		status = mpd_run_status(conn);
		if (status == NULL)
			printErrorAndExit(conn);

		current_id = mpd_status_get_update_id(status);
		mpd_status_free(status);

		/* is our last queued update finished now? */

		if (current_id == 0 || current_id > id ||
		    (id > 1 << 30 && id < 1000)) /* wraparound */
			break;
	}

	return 1;
}

int cmd_ls ( int argc, char ** argv, struct mpd_connection *conn )
{
	const char *ls;
	int i = 0;

	if (argc > 0)
		ls = charset_to_utf8(argv[i]);
	else
		ls = strdup("");

	do {
		if (!mpd_send_list_meta(conn, ls))
			printErrorAndExit(conn);

		print_entity_list(conn, MPD_ENTITY_TYPE_UNKNOWN);
		my_finishCommand(conn);

	} while (++i < argc && (ls = charset_to_utf8(argv[i])) != NULL);

	return 0;
}

int cmd_lsplaylists ( int argc, char ** argv, struct mpd_connection *conn )
{
	const char * ls = "";
	int i = 0;

	if(argc>0) ls = charset_to_utf8(argv[i]);

	do {
		if (!mpd_send_list_meta(conn, ls))
			printErrorAndExit(conn);

		print_entity_list(conn, MPD_ENTITY_TYPE_PLAYLIST);
		my_finishCommand(conn);

	} while (++i < argc && (ls = charset_to_utf8(argv[i])) != NULL);
	return 0;
}

int cmd_load ( int argc, char ** argv, struct mpd_connection *conn )
{
	int i;

	if (!mpd_command_list_begin(conn, false))
		printErrorAndExit(conn);

	for(i=0;i<argc;i++) {
		printf("loading: %s\n",argv[i]);
		mpd_send_load(conn, charset_to_utf8(argv[i]));
	}
	mpd_command_list_end(conn);
	my_finishCommand(conn);

	return 0;
}

int cmd_insert (int argc, char ** argv, struct mpd_connection *conn )
{
	int ret;
	struct mpd_status *status = mpd_run_status(conn);
	if (status == NULL)
		printErrorAndExit(conn);

	const int from = mpd_status_get_queue_length(status);

	ret = cmd_add(argc, argv, conn);
	const int cur_pos = mpd_status_get_song_pos(status);
	mpd_status_free(status);
	if (ret != 0) {
		return ret;
	}
	return mpd_run_move_range(conn, from, from+argc,
		cur_pos+1);
}

int cmd_list ( int argc, char ** argv, struct mpd_connection *conn )
{
	enum mpd_tag_type type;
	struct mpd_pair *pair;

	type = get_search_type(argv[0]);
	if (type == MPD_TAG_UNKNOWN)
		return -1;

	argc -= 1;
	argv += 1;

	mpd_search_db_tags(conn, type);

	if (argc > 0 && !add_constraints(argc, argv, conn))
		return -1;

	if (!mpd_search_commit(conn))
		printErrorAndExit(conn);

	while ((pair = mpd_recv_pair_tag(conn, type)) != NULL) {
		printf("%s\n", charset_from_utf8(pair->value));
		mpd_return_pair(conn, pair);
	}

	my_finishCommand(conn);

	return 0;
}

int cmd_volume ( int argc, char ** argv, struct mpd_connection *conn )
{
        struct int_value_change ch;
	struct mpd_status *status;

	if(argc==1) {
                if(!parse_int_value_change(argv[0], &ch))
			DIE("\"%s\" is not an integer\n", argv[0]);
	} else {
		status = getStatus(conn);

		if (mpd_status_get_volume(status) >= 0)
			printf("volume:%3i%c\n",
			       mpd_status_get_volume(status), '%');
		else
			printf("volume: n/a\n");

		mpd_status_free(status);

		return 0;
	}

	if (ch.is_relative) {
		int old_volume;

		status = getStatus(conn);
		old_volume = mpd_status_get_volume(status);
		mpd_status_free(status);

		ch.value += old_volume;
		if (ch.value < 0)
			ch.value = 0;
		else if (ch.value > 100)
			ch.value = 100;

		if (ch.value == old_volume)
			return 1;
	}

	if (!mpd_run_set_volume(conn, ch.value))
		printErrorAndExit(conn);
	return 1;
}

int
cmd_pause(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	mpd_send_pause(conn, true);
	my_finishCommand(conn);

	return 1;
}

int cmd_repeat ( int argc, char ** argv, struct mpd_connection *conn )
{
	int mode;

	if(argc==1) {
		mode = get_boolean(argv[0]);
		if (mode < 0)
			return -1;
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);
		mode = !mpd_status_get_repeat(status);
		mpd_status_free(status);
	}

	if (!mpd_run_repeat(conn, mode))
		printErrorAndExit(conn);

	return 1;
}

int cmd_random ( int argc, char ** argv, struct mpd_connection *conn )
{
	int mode;

	if(argc==1) {
		mode = get_boolean(argv[0]);
		if (mode < 0)
			return -1;
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);
		mode = !mpd_status_get_random(status);
		mpd_status_free(status);
	}

	if (!mpd_run_random(conn, mode))
		printErrorAndExit(conn);

	return 1;
}

int cmd_single ( int argc, char ** argv, struct mpd_connection *conn )
{
	int mode;

	if(argc==1) {
		mode = get_boolean(argv[0]);
		if (mode < 0)
			return -1;
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);
		mode = !mpd_status_get_single(status);
		mpd_status_free(status);
	}

	if (!mpd_run_single(conn, mode))
		printErrorAndExit(conn);

	return 1;
}

int cmd_consume ( int argc, char ** argv, struct mpd_connection *conn )
{
	int mode;

	if(argc==1) {
		mode = get_boolean(argv[0]);
		if (mode < 0)
			return -1;
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);
		mode = !mpd_status_get_consume(status);
		mpd_status_free(status);
	}

	if (!mpd_run_consume(conn, mode))
		printErrorAndExit(conn);

	return 1;
}

int cmd_crossfade ( int argc, char ** argv, struct mpd_connection *conn )
{
	int seconds;

	if(argc==1) {
                if(!parse_int(argv[0], &seconds) || seconds<0)
			DIE("\"%s\" is not 0 or positive integer\n",argv[0]);

		if (!mpd_run_crossfade(conn, seconds))
			printErrorAndExit(conn);
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);

		printf("crossfade: %i\n", mpd_status_get_crossfade(status));

		mpd_status_free(status);
	}
	return 0;
}

#if LIBMPDCLIENT_CHECK_VERSION(2,2,0)
int cmd_mixrampdb ( int argc, char ** argv, struct mpd_connection *conn )
{
	float db;

	if(argc==1) {
		if(!parse_float(argv[0], &db))
			DIE("\"%s\" is not a floating point number\n",argv[0]);

		mpd_run_mixrampdb(conn, db);
                my_finishCommand(conn);
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);

		printf("mixrampdb: %f\n", mpd_status_get_mixrampdb(status));

		mpd_status_free(status);
	}
	return 0;
}

int cmd_mixrampdelay ( int argc, char ** argv, struct mpd_connection *conn )
{
	float seconds;

	if(argc==1) {
		if(!parse_float(argv[0], &seconds))
			DIE("\"%s\" is not a floating point number\n",argv[0]);

		mpd_run_mixrampdelay(conn, seconds);
		my_finishCommand(conn);
	}
	else {
		struct mpd_status *status;
		status = getStatus(conn);

		printf("mixrampdelay: %f\n",
		       mpd_status_get_mixrampdelay(status));

		mpd_status_free(status);
	}
	return 0;
}
#endif

int
cmd_version(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	const unsigned *version = mpd_connection_get_server_version(conn);

	if (version != NULL)
		printf("mpd version: %i.%i.%i\n", version[0],
		       version[1], version[2]);
	else
		printf("mpd version: unknown\n");

	return 0;
}

int cmd_loadtab ( int argc, char ** argv, struct mpd_connection *conn )
{
	struct mpd_playlist *pl;

	if (argc != 1)
		return 0;

	if (!mpd_send_list_meta(conn, NULL))
		printErrorAndExit(conn);

	while ((pl = mpd_recv_playlist(conn)) != NULL) {
		if (strncmp(mpd_playlist_get_path(pl), argv[0],
			    strlen(argv[0])) == 0)
			printf("%s\n",
			       charset_from_utf8(mpd_playlist_get_path(pl)));

		mpd_playlist_free(pl);
	}

	my_finishCommand(conn);
	return 0;
}

int cmd_lstab ( int argc, char ** argv, struct mpd_connection *conn )
{
	struct mpd_directory *dir;

	if (argc != 1)
		return 0;

	if (!mpd_send_list_all(conn, NULL))
		printErrorAndExit(conn);

	while ((dir = mpd_recv_directory(conn)) != NULL) {
		if (strncmp(mpd_directory_get_path(dir), argv[0],
			    strlen(argv[0])) == 0)
			printf("%s\n",
			       charset_from_utf8(mpd_directory_get_path(dir)));

		mpd_directory_free(dir);
	}

	my_finishCommand(conn);

	return 0;
}

int cmd_tab ( int argc, char ** argv, struct mpd_connection *conn )
{
	struct mpd_song *song;
	char empty[] = "";
	char *dir = empty;
	char *tmp = NULL;

	if (argc == 1) {
		if (strrchr(argv[0], '/')) {
			dir = strdup(argv[0]);
			if (!dir) return 0;
			tmp = strrchr(dir, '/');
			if (tmp) *tmp = '\0'; // XXX: It's unpossible for tmp to be NULL.
		}
	}

	if (!mpd_send_list_all(conn, dir))
		printErrorAndExit(conn);

	if (*dir) free(dir);

	while ((song = mpd_recv_song(conn)) != NULL) {
		if (argc != 1 ||
		    strncmp(mpd_song_get_uri(song), argv[0],
			    strlen(argv[0])) == 0)
			printf("%s\n",
			       charset_from_utf8(mpd_song_get_uri(song)));

		mpd_song_free(song);
	}

	my_finishCommand(conn);
	return 0;
}

static char * DHMS(unsigned long t)
{
	static char buf[32];	/* Ugh */
	int days, hours, mins, secs;

#ifndef SECSPERDAY
#define SECSPERDAY 86400
#endif
#ifndef SECSPERHOUR
#define SECSPERHOUR 3600
#endif
#ifndef SECSPERMIN
#define SECSPERMIN 60
#endif

	days = t / SECSPERDAY;
	t %= SECSPERDAY;
	hours = t / SECSPERHOUR;
	t %= SECSPERHOUR;
	mins = t / SECSPERMIN;
	t %= SECSPERMIN;
	secs = t;

	snprintf(buf, sizeof(buf) - 1, "%d days, %d:%02d:%02d",
	    days, hours, mins, secs);
	return buf;
}

int
cmd_stats(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	struct mpd_stats *stats;
	time_t t;

	stats = mpd_run_stats(conn);
	if (stats == NULL)
		printErrorAndExit(conn);

	t = mpd_stats_get_db_update_time(stats);
	printf("Artists: %6d\n", mpd_stats_get_number_of_artists(stats));
	printf("Albums:  %6d\n", mpd_stats_get_number_of_albums(stats));
	printf("Songs:   %6d\n", mpd_stats_get_number_of_songs(stats));
	printf("\n");
	printf("Play Time:    %s\n", DHMS(mpd_stats_get_play_time(stats)));
	printf("Uptime:       %s\n", DHMS(mpd_stats_get_uptime(stats)));
	printf("DB Updated:   %s", ctime(&t));	/* no \n needed */
	printf("DB Play Time: %s\n", DHMS(mpd_stats_get_db_play_time(stats)));

	mpd_stats_free(stats);
	return 0;
}

int
cmd_status(mpd_unused  int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
	if (options.verbosity >= V_DEFAULT)
		print_status(conn);
	return 0;
}

int
cmd_replaygain(int argc, char **argv, struct mpd_connection *connection)
{
	/* libmpdclient 2.0 doesn't support these commands yet, we
	   have to roll our own with mpd_send_command() */

	if (mpd_connection_cmp_server_version(connection, 0, 16, 0) < 0)
		fprintf(stderr, "warning: MPD 0.16 required for this command\n");

	if (argc == 0) {
		struct mpd_pair *pair;

		mpd_send_command(connection, "replay_gain_status", NULL);
		while ((pair = mpd_recv_pair(connection)) != NULL) {
			printf("%s: %s\n", pair->name, pair->value);
			mpd_return_pair(connection, pair);
		}

		my_finishCommand(connection);
	} else {
		mpd_send_command(connection, "replay_gain_mode",
				 argv[0], NULL);
		my_finishCommand(connection);
	}

	return 0;
}


/** My Part **/

enum my_menu_id
{
  MENU_MAIN,
  //	MENU_WAIT,
  //	MENU_PLAYLIST,
  //	MENU_QUEUE,
  MENU_NUMBER
};

struct VerboseArgs
{
  struct mpd_connection *conn;
  /** set to 1 once commands have been triggered by keyboad*/
  int new_command_signal;
  int menu_id;
};

static pthread_mutex_t conn_mutex;
#define LOCK_FRAGILE pthread_mutex_lock(&conn_mutex);
#define UNLOCK_FRAGILE pthread_mutex_unlock(&conn_mutex);

static int
check_new_state(struct VerboseArgs *vargs)
{
  static int repeat, randomm, single, queue_len, id, volume, play,
	rep, ran, sin, que, idd, vol, ply;
  struct mpd_status *status;
  
  if(vargs->new_command_signal == 0)
	{
	  status = getStatus(vargs->conn);
	  rep = mpd_status_get_repeat(status);
	  ran = mpd_status_get_random(status);
	  sin = mpd_status_get_single(status);
	  que = mpd_status_get_queue_length(status);
	  idd = mpd_status_get_song_id(status);
	  vol = mpd_status_get_volume(status);
	  ply = mpd_status_get_state(status);
	  mpd_status_free(status);
	  if(rep != repeat || ran != randomm || sin != single
		 || que != queue_len || idd != id || vol != volume
		 || ply != play)
		{
		  repeat = rep;
		  randomm = ran;
		  single = sin;
		  queue_len = que;
		  id = idd;
		  volume = vol;
		  play = ply;
		  return 2;
		}
		
	  return 0;
	}
  else // new local command triggered by key strike
	vargs->new_command_signal = 0;
  
  return 1;
}

static void
print_basic_help(void)
{
  move(5, 0);
  printw("  [n] :\tNext song\t\t  [R] :\tToggle repeat\n");
  printw("  [p] :\tPrevious song\t\t  [r] :\tToggle random\n");
  printw("  [-] :\tVolume down\t\t  [s] :\tToggle single\n");
  printw("  [=] :\tVolume Up\t\t  [b] :\tPlayback\n");
  printw("  [t] :\tPlay / Pause\t\t  [l] :\tRedraw screen\n");
  printw("  <L> :\tSeek backward\t\t  <R> :\tSeek forward\n");
  printw("\n  [e/q] : Quit\n");
}

static void
print_basic_song_info(struct mpd_connection* conn)
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

  clear();
	  
  if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
	  mpd_status_get_state(status) == MPD_STATE_PAUSE) {
	struct mpd_song *song;

	if (!mpd_response_next(conn))
	  printErrorAndExit(conn);

	song = mpd_recv_song(conn);
	if (song != NULL) {
	  // pretty_print_song with ncurses portability
	  printw("%s\n",
			 songToFormatedString(song, options.format, NULL));

	  mpd_song_free(song);
	}

	if (mpd_status_get_state(status) == MPD_STATE_PLAY)
	  printw("[playing]");
	else
	  printw("[paused] ");

	printw("     #%3i/%3u      %i:%02i\n",
		   mpd_status_get_song_pos(status) + 1,
		   mpd_status_get_queue_length(status),
		   mpd_status_get_total_time(status) / 60,
		   mpd_status_get_total_time(status) % 60
		   );
  }
	
  if (mpd_status_get_update_id(status) > 0)
	printw("Updating DB (#%u) ...\n",
		   mpd_status_get_update_id(status));

  if (mpd_status_get_volume(status) >= 0)
	printw("volume:%3i%c   ", mpd_status_get_volume(status), '%');
  else {
	printw("volume: n/a   ");
  }

  printw("Repeat: ");
  if (mpd_status_get_repeat(status))
	printw("on    ");
  else printw("off   ");

  printw("random: ");
  if (mpd_status_get_random(status))
	printw("on    ");
  else printw("off   ");

  printw("single: ");
  if (mpd_status_get_single(status))
	printw("on\n");
  else printw("off\n");

  if (mpd_status_get_error(status) != NULL)
	printw("ERROR: %s\n",
		   charset_from_utf8(mpd_status_get_error(status)));

  print_basic_help();
  refresh();

  mpd_status_free(status);
  my_finishCommand(conn);
}

static void
print_basic_bar(struct mpd_connection *conn)
{
  int crt_time, crt_time_perc, total_time,
	fill_len, empty_len, i, bit_rate;
  static int last_bit_rate = 0;
  const int axis_len = 40;
  struct mpd_status *status;

  status = getStatus(conn);
  crt_time = mpd_status_get_elapsed_time(status);
  total_time = mpd_status_get_total_time(status);
  bit_rate = mpd_status_get_kbit_rate(status);
  mpd_status_free(status);

  /* as many songs's bit rate varies while playing
	 we refresh the bit rate display only when the 
	 relevant change greater than 0.2 */
  if(abs(bit_rate - last_bit_rate) / (float)(last_bit_rate + 1) > 0.2)
	last_bit_rate = bit_rate;
  else
	bit_rate = last_bit_rate;

  crt_time_perc = (total_time == 0 ? 0 : 100 * crt_time / total_time);  
  fill_len = crt_time_perc * axis_len / 100;
  empty_len = axis_len - fill_len;

  move(3, 0);
  printw("\r");
  printw("[");
  for(i = 0; i < fill_len; printw("="), i++);
  printw(">");
  for(i = 0; i < empty_len; printw(" "), i++);
  printw("]");
  
  printw("%3i%% %5ik/s %3i:%02i/%i:%02i%*s",
		 crt_time_perc,
		 bit_rate,
		 crt_time / 60,
		 crt_time % 60,
		 total_time / 60,
		 total_time % 60,
		 8, " ");

  refresh();
}

static void
redraw_screen(struct mpd_connection *conn)
{
  print_basic_song_info(conn);
  print_basic_bar(conn);
}

#define INTERVAL_UNIT 30000

static void
menu_main_print_routine(struct VerboseArgs *vargs)
{
  if(check_new_state(vargs))
	{
	  // refresh the status display
	  redraw_screen(vargs->conn);
	}

  print_basic_bar(vargs->conn);
}

#define SEEK_UNIT 3
#define VOLUME_UNIT 3

static void
cmd_forward(mpd_unused int argc, mpd_unused char **argv,
			struct mpd_connection  *conn)
{
  char *args = malloc(8 * sizeof(char));
  sprintf(args, "+%i%%", SEEK_UNIT);
  cmd_seek(1, &args, conn);
  free(args);
}

static void
cmd_backward(mpd_unused int argc, mpd_unused char **argv,
			 struct mpd_connection  *conn)
{
  char *args = malloc(8 * sizeof(char));
  sprintf(args, "-%i%%", SEEK_UNIT);
  cmd_seek(1, &args, conn);
  free(args);
}

static int
cmd_volup(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
  struct mpd_status *status;
  int volume;

  status = getStatus(conn);
  volume = mpd_status_get_volume(status);
  mpd_status_free(status);

  volume += VOLUME_UNIT;
  if(volume > 100)
	volume = 100;

  if (!mpd_run_set_volume(conn, volume))
	printErrorAndExit(conn);
  return 1;
}
  
static int
cmd_voldown(mpd_unused int argc, mpd_unused char **argv, struct mpd_connection *conn)
{
  struct mpd_status *status;
  int volume;

  status = getStatus(conn);
  volume = mpd_status_get_volume(status);
  mpd_status_free(status);

  volume -= VOLUME_UNIT;
  if(volume < 0)
	volume = 0;

  if (!mpd_run_set_volume(conn, volume))
	printErrorAndExit(conn);
  return 1;
}

#define CASE_EXCUTE(command_name)				\
  LOCK_FRAGILE								\
  command_name(0, NULL, vargs->conn);			\
  vargs->new_command_signal = 1;				\
  UNLOCK_FRAGILE								\
  break;

static int
menu_main_keyboard(struct VerboseArgs *vargs)
{
  switch(getch())
	{
	case '+': ;
	case '=': ;
	  CASE_EXCUTE(cmd_volup)
	case KEY_RIGHT:
		CASE_EXCUTE(cmd_forward)
	case '-':
		CASE_EXCUTE(cmd_voldown)
	case KEY_LEFT:
		CASE_EXCUTE(cmd_backward)
	case 'b':
		CASE_EXCUTE(cmd_playback)
	case 'n':
		CASE_EXCUTE(cmd_next)
	case 'p':
		CASE_EXCUTE(cmd_prev)
	case 't': ;
	case ' ':
	  CASE_EXCUTE(cmd_toggle)
	case 'r':
	  CASE_EXCUTE(cmd_random)
	case 's':
	  CASE_EXCUTE(cmd_single)
	case 'R':
	  CASE_EXCUTE(cmd_repeat)
	case 'l':
	  LOCK_FRAGILE
		vargs->new_command_signal = 1;
	  UNLOCK_FRAGILE
		break;
	case 'e': ;
	case 'q':
	  return 1;
	default: ;
	}
  
  return 0;
}

static void*
menu_rendering(void *args)
{
  struct VerboseArgs *vargs;

  vargs = (struct VerboseArgs*)args;
  for(;;)
	{
	  LOCK_FRAGILE

	  if(vargs->menu_id == MENU_MAIN)
		menu_main_print_routine(vargs);

	  UNLOCK_FRAGILE

	  usleep(INTERVAL_UNIT);
	}

  return NULL;
}

static void
verbose_args_init(struct VerboseArgs *vargs, struct mpd_connection *conn)
{
  vargs->conn = conn;
  vargs->menu_id = MENU_MAIN;
  /* initialization require redraw too */
  vargs->new_command_signal = 1;
}

static void
verbose_args_destroy(mpd_unused struct VerboseArgs *vargs)
{
  return;
}

int
cmd_dynamic(mpd_unused int argc, mpd_unused char **argv,
			struct mpd_connection *conn)
{
  pthread_t thread_menu_rendering;
  int rc, is_quit;
  struct VerboseArgs vargs;

  // ncurses basic setting
  initscr();
  timeout(-1); // no time out for keyboard stroke
  curs_set(0); // cursor invisible
  noecho();
  keypad(stdscr, TRUE); // enable getch() get KEY_UP/DOWN

  // ncurses for unicode support
  setlocale(LC_ALL, "");

  pthread_mutex_init(&conn_mutex, NULL);

  verbose_args_init(&vargs, conn);
  
  rc = pthread_create(&thread_menu_rendering, NULL,
					  menu_rendering, (void*)&vargs);
  if(rc)
	{
	  endwin();
	  DIE("thread init failed,\nexit...\n");
	}

  /** main loop for keyboard hit daemon */
  for(;;)
	{
	  if(vargs.menu_id == MENU_MAIN)
		is_quit = menu_main_keyboard(&vargs);
	  /* else if(menu_id == MENU_PLAYLIST) */
	  /* 	is_quit = menu_playlist_keyboard(vargs); */

	  if(is_quit)
		{
		  pthread_cancel(thread_menu_rendering);
		  break;
		}

	  usleep(INTERVAL_UNIT);	  
	}

  endwin();
  
  pthread_join(thread_menu_rendering, NULL);
  pthread_mutex_destroy(&conn_mutex);

  verbose_args_destroy(&vargs);

  return 0;
}
