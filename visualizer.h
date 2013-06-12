#include "global.h"
#include "windows.h"

struct Visualizer
{
  int fifo_id;
  char fifo_file[64];
  int16_t buff[BUFFER_SIZE];
};

void get_fifo_id(void);
mpd_unused int get_fifo_output_id(void);
mpd_unused void fifo_output_update(void);
void print_uv_meter(int bars, int max);
void draw_sound_wave(int16_t *buf);
void print_visualizer(void);

