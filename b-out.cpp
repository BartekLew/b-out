#include <SDL.h>
#include <cstdlib>

void fatal() {
	fprintf (stderr, "b-out: %s\n", SDL_GetError());
	SDL_Quit();
   	exit(EXIT_FAILURE);
}

class Drawable {
	public:
	virtual void draw(SDL_Renderer *renderer) = 0;
};

class Playground {
    public:
    Playground(uint width, uint height) {
        if(SDL_Init(SDL_INIT_VIDEO) < 0) fatal();
	    
		if(SDL_CreateWindowAndRenderer(
			width, height, 0, &window, &renderer
		) != 0) fatal();

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);

		SDL_RenderPresent(renderer);
    }

	~Playground() {
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	void put(Drawable &d) {
		d.draw(renderer);
		SDL_RenderPresent(renderer);
	}

	private:
	SDL_Window *window;
	SDL_Renderer *renderer;
};

class Ball : public Drawable {
	public:

	void draw(SDL_Renderer *renderer) {
		SDL_SetRenderDrawColor(renderer, red, green, blue, 255);	

		for(uint dy = 1; dy < r; dy++) {
			/* Draw circle line by line.
			 *
			 * Formula (for point 0,0) is: r^2 = x^2 + y^2
			 *
			 * now we have y and r known, so:
			 * x = +/- sqrt(r^2 - y^2)
			 *
			 * Of course we have to apply offset (x,y),
			 * so I operate on dx and dy rather than x & y.*/

			uint dx = floor(sqrt(r*r - dy*dy));
			SDL_RenderDrawLine(renderer,
				x0 - dx, y0 - dy, x0 + dx, y0 - dy
			);
			SDL_RenderDrawLine(renderer,
				x0 - dx, y0 + dy, x0 + dx, y0 + dy
			);
		}

		SDL_RenderDrawLine(renderer, x0 - r, y0, x0 + r, y0);
	}

	Ball& at(uint x, uint y) {
		x0 = x; y0 = y;
		return *this;
	}

	private:
	uint red = 0xff, green = 0xff, blue=0,
		 x0 = 400, y0 = 300, r = 10;
};
int main(int argc, char **argv) {
	Playground pg(800,600);
	Ball ball;

	pg.put(ball.at(300,300));

	SDL_Delay(2000);
}
