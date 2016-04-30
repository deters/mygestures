#ifndef BRUSH_IMAGE
#define BRUSH_IMAGE

struct brush_image_t {
	unsigned int width;
	unsigned int height;
	unsigned int bytes_per_pixel; /* 3:RGB, 4:RGBA */
	unsigned char pixel_data[7 * 7 * 4 + 1];
};

extern struct brush_image_t *brush_image;
extern struct brush_image_t brush_image_blue;
extern struct brush_image_t brush_image_red;
extern struct brush_image_t brush_image_purple;
extern struct brush_image_t brush_image_green;
extern struct brush_image_t brush_image_white;
extern struct brush_image_t brush_image_yellow;

#endif
