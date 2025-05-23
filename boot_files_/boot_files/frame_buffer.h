void frame_buffer_init();
void draw_pixel(int x, int y, unsigned char attr);
void draw_char(unsigned char ch, int x, int y, unsigned char attr);
void draw_char_scaled(unsigned char ch, int x, int y, unsigned char attr, int scale);
void draw_string(int x, int y, char* s, unsigned char attr);
void draw_string_scaled(int x, int y, char* s, unsigned char attr, int scale);