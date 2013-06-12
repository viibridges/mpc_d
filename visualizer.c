#include "visualizer.h"
#include "windows.h"

extern struct mpd_connection *conn;
extern struct Visualizer *visualizer;

/** Music Visualizer **/
void
get_fifo_id(void)
{
  const char *fifo_path = visualizer->fifo_file;
  int id = -1;
  
  if((id = open(fifo_path , O_RDONLY | O_NONBLOCK)) < 0)
	debug("couldn't open the fifo file");

  visualizer->fifo_id = id;
}

mpd_unused int
get_fifo_output_id(void)
{
	struct mpd_output *output;
	int ret = -1, id;
	const char *name;

	mpd_send_outputs(conn);

	while ((output = mpd_recv_output(conn)) != NULL) {
		id = mpd_output_get_id(output);
		name = mpd_output_get_name(output);

		if (mpd_output_get_enabled(output)
			&& !strstr(name, "FIFO"))
		  ret = id;

		mpd_output_free(output);
	}

	if(ret == -1)
	  debug("output disenabled.");
	
	mpd_response_finish(conn);

	return ret;
}


mpd_unused void
fifo_output_update(void)
{
  int fifo_output_id = get_fifo_output_id();

  if(fifo_output_id != -1)
	{
	  mpd_run_disable_output(conn, fifo_output_id);
	  usleep(3e4);
	  mpd_run_enable_output(conn, fifo_output_id);
	}
}

void
print_uv_meter(int bars, int max)
{
  int i, long_tail = 0;
  
  WINDOW *win = specific_win(VISUALIZER);

  int width = win->_maxx - 1;

  if(max == 0)
	bars = max = 1;
  
  while(bars > width)
	{
	  max = bars - width;
	  bars = width - max;
	  long_tail = 1;
	}
  
  if(max > width)
	max = width;

  wprintw(win, "_");

  if(long_tail)
	{
	  const char *trail = "-/|\\";
	  static int j = 0;

	  wattron(win, my_color_pairs[0]);
	  for(i = 0; i < bars; i++)
		wprintw(win, "%c", trail[j]);
	  wattron(win, my_color_pairs[0]);

	  wattron(win, my_color_pairs[2]);
	  for(i = 0; i < max; i++)
		wprintw(win, "%c", trail[j]);
	  wattroff(win, my_color_pairs[2]);	  
	  j = (j + 1) % 4;
	}
  else
	{
	  wattron(win, my_color_pairs[0]);
	  for(i = 0; i < bars; i++)
		wprintw(win, "|");
	  wattroff(win, my_color_pairs[0]);  
	  wmove(win, 0, max);
	  color_print(win, 3, "+");
	}

  mvwprintw(win, 0, win->_maxx, "_");
}

void
draw_sound_wave(int16_t *buf)
{
  int i;
  double energy = .0, scope = 30.;
  const int size = BUFFER_SIZE;

  const int width = wchain[VISUALIZER].win->_maxx - 1;
  
  for(i = 0; i < size; i++)
	energy += buf[i] * buf[i];
	/* if(energy < buf[i] * buf[i]) */
	/*   energy = buf[i] * buf[i]; */

  /** filter the sound energy **/
  energy = pow(sqrt(energy / size), 1. / 3.);
  
  int bars = energy * width / scope;

  static int last_bars = 0, max = 0;
  const int brake = 1;

  if(bars - last_bars > brake)
	bars -= brake;
  else
	bars = last_bars - brake;

  last_bars = bars;
  
  if(max < bars || max > bars + 10)
	max = bars;
  if(bars < 2)
	max = bars;

  /* it seems that the mpd fifo a little percivably
	 forward than the sound card rendering, I use
	 cache to synchronize them */
#define DELAY 15
  static int bcache[DELAY], mcache[DELAY], id = 0;
  bcache[id] = bars, mcache[id] = max;
  id = (id + 1) % DELAY;
  print_uv_meter(bcache[id], mcache[id]);
  
  //print_uv_meter(bars, max);
}

void
print_visualizer(void)
{
  int fifo_id = visualizer->fifo_id;
  int16_t *buf = visualizer->buff;

  if (fifo_id < 0)
	return;

  if(read(fifo_id, buf, BUFFER_SIZE *
		  sizeof(int16_t) / sizeof(char)) < 0)
	return;

  static long count = 0;
  // some delicate calculation of the refresh interval 
  if(count++ % 4000 == 0)
	{
	  fifo_output_update();
	  count = 1;
	}
  
  draw_sound_wave(buf);
  interval_level = 1;
}
