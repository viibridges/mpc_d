/* music player command (mpc)
 * Copyright (C) 2003-2008 Warren Dukes <warren.dukes@gmail.com>,
				Eric Wong <normalperson@yhbt.net>,
				Daniel Brown <danb@cs.utexas.edu>
 * Copyright (C) 2008-2011 Max Kellermann <max@duempel.org>
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

#ifndef MPC_MESSAGE_H
#define MPC_MESSAGE_H

struct mpd_connection;

int
cmd_channels(int argc, char **argv, struct mpd_connection *connection);

int
cmd_sendmessage(int argc, char **argv, struct mpd_connection *connection);

int
cmd_waitmessage(int argc, char **argv, struct mpd_connection *connection);

int
cmd_subscribe(int argc, char **argv, struct mpd_connection *connection);

#endif /* MPC_MESSAGE_H */
