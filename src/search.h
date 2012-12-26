/*
 * Copyright (C) 2008-2010 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPC_SEARCH_H
#define MPC_SEARCH_H

#include <mpd/client.h>

struct mpd_connection;

enum {
	SEARCH_TAG_ANY = MPD_TAG_COUNT + 1,
	SEARCH_TAG_URI = MPD_TAG_COUNT + 2,
};

struct constraint {
	enum mpd_tag_type type;
	char *query;
};

enum mpd_tag_type
get_search_type(const char *name);

int
get_constraints(int argc, char **argv, struct constraint **constraints);

bool
add_constraints(int argc, char ** argv, struct mpd_connection *conn);

int
cmd_search(int argc, char **argv, struct mpd_connection *conn);

int
cmd_search_type(int argc, char **argv, struct mpd_connection *conn, const char *search_type);

int
cmd_search_title(int argc, char **argv, struct mpd_connection *conn);

int
cmd_search_artist(int argc, char **argv, struct mpd_connection *conn);

int
cmd_search_album(int argc, char **argv, struct mpd_connection *conn);

int
cmd_find(int argc, char **argv, struct mpd_connection *conn);

int
cmd_findadd(int argc, char **argv, struct mpd_connection *conn);

#endif
