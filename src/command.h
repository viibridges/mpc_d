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

#ifndef COMMAND_H
#define COMMAND_H

struct mpd_connection;
enum mpd_tag_type;

int cmd_status(int argc, char **argv, struct mpd_connection *conn);
int cmd_add(int argc, char **argv, struct mpd_connection *conn);
int cmd_crop(int argc, char **argv, struct mpd_connection *conn);
int cmd_current(int argc, char **argv, struct mpd_connection *conn);
int cmd_del(int argc, char **argv, struct mpd_connection *conn);
int cmd_play(int argc, char **argv, struct mpd_connection *conn);
int cmd_next(int argc, char **argv, struct mpd_connection *conn);
int cmd_prev(int argc, char **argv, struct mpd_connection *conn);
int cmd_pause(int argc, char **argv, struct mpd_connection *conn);
int cmd_stop(int argc, char **argv, struct mpd_connection *conn);
int cmd_seek(int argc, char **argv, struct mpd_connection *conn);
int cmd_clear(int argc, char **argv, struct mpd_connection *conn);
int cmd_shuffle(int argc, char **argv, struct mpd_connection *conn);
int cmd_clearerror(int argc, char **argv, struct mpd_connection *conn);
int cmd_move(int argc, char **argv, struct mpd_connection *conn);
int cmd_playlist(int argc, char **argv, struct mpd_connection *conn);
int cmd_listall(int argc, char **argv, struct mpd_connection *conn);
int cmd_ls(int argc, char **argv, struct mpd_connection *conn);
int cmd_lsplaylists(int argc, char **argv, struct mpd_connection *conn);
int cmd_load(int argc, char **argv, struct mpd_connection *conn);
int cmd_insert(int argc, char **argv, struct mpd_connection *conn);
int cmd_list(int argc, char **argv, struct mpd_connection *conn);
int cmd_save(int argc, char **argv, struct mpd_connection *conn);
int cmd_rm(int argc, char **argv, struct mpd_connection *conn);
int cmd_volume(int argc, char **argv, struct mpd_connection *conn);
int cmd_repeat(int argc, char **argv, struct mpd_connection *conn);
int cmd_random(int argc, char **argv, struct mpd_connection *conn);
int cmd_single(int argc, char **argv, struct mpd_connection *conn);
int cmd_consume(int argc, char **argv, struct mpd_connection *conn);
int cmd_crossfade(int argc, char **argv, struct mpd_connection *conn);
int cmd_mixrampdb(int argc, char **argv, struct mpd_connection *conn);
int cmd_mixrampdelay(int argc, char **argv, struct mpd_connection *conn);
int cmd_enable( int argc, char **argv, struct mpd_connection *conn);
int cmd_disable( int argc, char **argv, struct mpd_connection *conn);
int cmd_outputs(int argc, char **argv, struct mpd_connection *conn);
int cmd_update(int argc, char **argv, struct mpd_connection *conn);
int cmd_version(int argc, char **argv, struct mpd_connection *conn);
int cmd_loadtab(int argc, char **argv, struct mpd_connection *conn);
int cmd_lstab(int argc, char **argv, struct mpd_connection *conn);
int cmd_tab(int argc, char **argv, struct mpd_connection *conn);
int cmd_stats(int argc, char **argv, struct mpd_connection *conn);
int cmd_toggle(int argc, char **argv, struct mpd_connection *conn);
int cmd_search_general(int argc, char **argv,
					   struct mpd_connection *conn, enum mpd_tag_type search_type);
int cmd_search_title( int argc, char ** argv, struct mpd_connection *conn);
int cmd_search_artist( int argc, char ** argv, struct mpd_connection *conn);
int cmd_search_album( int argc, char ** argv, struct mpd_connection *conn);
int cmd_list_around( int argc,  char ** argv, struct mpd_connection *conn);
int cmd_illustrate( int argc,  char ** argv, struct mpd_connection *conn);
int cmd_playback( int argc,  char ** argv, struct mpd_connection *conn);
int cmd_control( int argc,  char ** argv, struct mpd_connection *conn);

int
cmd_replaygain(int argc, char **argv, struct mpd_connection *conn);

int
cmd_channels(int argc, char **argv, struct mpd_connection *conn);

int
cmd_sendmessage(int argc, char **argv, struct mpd_connection *conn);

int
cmd_waitmessage(int argc, char **argv, struct mpd_connection *conn);

int
cmd_subscribe(int argc, char **argv, struct mpd_connection *conn);

#endif /* COMMAND_H */
