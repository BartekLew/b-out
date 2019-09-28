#include <SDL.h>
#include <cstdlib>

void fatal() {
	fprintf (stderr, "b-out: %s\n", SDL_GetError());
	SDL_Quit();
   	exit(EXIT_FAILURE);
}

class Playground {
    public:
    Playground(uint width, uint height) {
        if(SDL_Init(SDL_INIT_VIDEO) < 0) fatal();
	    
		window = SDL_CreateWindow(
		    "b-out", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			width, height, SDL_WINDOW_SHOWN
		);
		if(window == NULL) fatal();

		surface = SDL_GetWindowSurface(window);
		SDL_FillRect(surface, NULL,
			SDL_MapRGB(surface->format, 0, 0, 0)
		);
		SDL_UpdateWindowSurface(window);
    }

	~Playground() {
		SDL_DestroyWindow(window);
		SDL_Quit();
	}


	private:
	SDL_Window *window;
	SDL_Surface *surface;
};

int main(int argc, char **argv) {
		Playground pg(800,600);
		SDL_Delay(2000);
}
