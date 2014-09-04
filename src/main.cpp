#include "SDL2.hpp"

#include <chrono>
#include <string>
#include <cstdlib>
#include <iomanip>
#include <cairo.h>
#include <cairo-gl.h>
#include <GL/glu.h>

// Size of the surface.
const unsigned int WIDTH = 512;
const unsigned int HEIGHT = 512;

inline void draw(cairo_surface_t* surface) {
	static double s = 1.0;

	cairo_t* cr = cairo_create(surface);

	cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);
	cairo_paint(cr);
	cairo_translate(cr, WIDTH / 2, HEIGHT / 2);
	cairo_scale(cr, s, s);
	cairo_arc(cr, 0.0, 0.0, WIDTH / 4, 0.0, 2.0 * 3.14159);
	cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
	cairo_fill(cr);
	cairo_surface_flush(surface);
	cairo_destroy(cr);

	s += 1.0 / 180.0;

	if(s >= 2.0) s = 1.0;
}

// TODO: Is unsigned long the right return type?
template<typename time>
unsigned long timediff(const time& start) {
	auto end = std::chrono::system_clock::now();

	return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

int main(int argc, char** argv) {
	if(argc != 3) {
		std::cerr
			<< "Usage: " << argv[0]
			<< " num_draws [image | gl | gl_texture]" << std::endl
		;

		return 1;
	}

	SDL2Window window;

	// if(!window.init(WIDTH, HEIGHT, SDL_WINDOW_HIDDEN)) {
	if(!window.init(WIDTH, HEIGHT)) {
		std::cerr << "Couldn't initialize SDL2 window; fatal." << std::endl;

		return 2;
	}

	if(window.makeCurrent()) {
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0.0, 0.0, WIDTH, HEIGHT);
		glClearColor(0.0, 0.1, 0.2, 1.0);
	}

    cairo_device_t* device = cairo_glx_device_create(
		window.getDisplay(),
		reinterpret_cast<GLXContext>(window.getCairoContext())
	);

	if(!device) {
		std::cerr << "Couldn't create device; fatal." << std::endl;

		return 3;
	}

	// Variable initialization. In the future, I'll need to setup
	// the OpenGL texture to use here as well.
	auto num_draws = std::atoi(argv[1]);
	auto method = std::string(argv[2]);
	cairo_surface_t* surface = nullptr;

	// TODO: Outside so it can be used...
	GLuint texture = 0;

	if(method == "image" ) surface = cairo_image_surface_create(
		CAIRO_FORMAT_ARGB32,
		WIDTH,
		HEIGHT
	);

	else if(method == "gl") surface = cairo_gl_surface_create(
		device,
		CAIRO_CONTENT_COLOR_ALPHA,
		WIDTH,
		HEIGHT
	);

	// TODO: Implement cairo_gl_surface_create_for_texture test.
	else if(method == "gl_texture") {
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGBA,
			WIDTH,
			HEIGHT,
			0,
			GL_BGRA_EXT,
			GL_UNSIGNED_BYTE,
			nullptr
		);

		surface = cairo_gl_surface_create_for_texture(
			device,
			CAIRO_CONTENT_COLOR_ALPHA,
			texture,
			WIDTH,
			HEIGHT
		);
	}

	else {
		std::cerr << "Unknown surface type '" << method << "'; fatal." << std::endl;

		return 4;
	}

	if(!surface) {
		std::cerr << "Couldn't create surface; fatal." << std::endl;

		return 5;
	}

	std::cout << "Performing " << num_draws << " iterations: " << std::flush;

	auto start = std::chrono::system_clock::now();
	auto last_tick = 0;

	for(auto i = 0; i < num_draws; i++) {
		draw(surface);

		// This is a completely wretched way of doing a progress meter,
		// but it's the best I'm willing to do for now.
		double pct = (static_cast<double>(i) / static_cast<double>(num_draws)) * 100.0;

		if(pct >= last_tick + 10.0) {
			std::cout << "+" << std::flush;

			last_tick = pct;
		}
	}

	std::cout << " done! (" << timediff(start) << "ms)" << std::endl;

	unsigned long frames = 0;
	unsigned long cairoTime = 0;
	unsigned long sdlTime = 0;

	auto drawStart = std::chrono::system_clock::now();

	window.main([&]() {
		start = std::chrono::system_clock::now();

		if(window.makeCairoCurrent()) {
			draw(surface);

			cairo_gl_surface_swapbuffers(surface);
		}

		cairoTime += timediff(start);

		start = std::chrono::system_clock::now();

		if(window.makeCurrent()) {
			int x = 0;
			int y = 0;
			int width = 512;
			int height = 512;

			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			gluOrtho2D(0.0, WIDTH, 0.0, HEIGHT);
			glMatrixMode(GL_MODELVIEW);
			glLoadIdentity();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glBindTexture(GL_TEXTURE_2D, texture);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glBegin(GL_QUADS);

			// Bottom-Left
			glTexCoord2i(0, 1);
			glVertex2i(x, y);

			// Upper-Left
			glTexCoord2i(0, 0);
			glVertex2i(x, y + height);

			// Upper-Right
			glTexCoord2i(1, 0);
			glVertex2i(x + width, y + height);

			// Bottom-Right
			glTexCoord2i(1, 1);
			glVertex2i(x + width, y);

			glEnd();
		}

		sdlTime += timediff(start);

		frames++;
	});

	auto fps = frames / (timediff(drawStart) / 1000);

	std::cout << "FPS: " << fps << std::endl;
	std::cout << "Cairo average time: " << cairoTime / frames << "ms" << std::endl;
	std::cout << "SDL2 average time: " << sdlTime / frames << "ms" << std::endl;

	cairo_surface_destroy(surface);
	cairo_device_destroy(device);

	window.deinit();

	return 0;
}

