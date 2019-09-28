#include <SDL.h>
#include <vector>
#include <cstdlib>

using namespace std;

void fatal() {
	fprintf (stderr, "b-out: %s\n", SDL_GetError());
	SDL_Quit();
   	exit(EXIT_FAILURE);
}

class Playground;

class Toy {
	public:
	virtual void draw(SDL_Renderer *renderer) = 0;
	virtual void on_dt(Playground &pg, uint dt) = 0;
};

class Playground {
    public:
    Playground(uint width, uint height) {
        if(SDL_Init(SDL_INIT_VIDEO) < 0) fatal();
	    
		if(SDL_CreateWindowAndRenderer(
			width, height, 0, &window, &renderer
		) != 0) fatal();

		newFrame();
		show();
    }

	~Playground() {
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	Playground& with(Toy &d) {
		toys.push_back(&d);
		d.draw(renderer);
		SDL_RenderPresent(renderer);

		return *this;
	}

	void play() {
		bool done = false;
		while(!done) {
			SDL_Event e;
			while(SDL_PollEvent(&e) != 0) {
				if(e.type == SDL_KEYDOWN)
					done = true;
			}

			newFrame();
			for(Toy *toy : toys) {
				toy->on_dt(*this, 1);
				toy->draw(renderer);
			}
			show();
		}
	}

	protected:
	void newFrame() {
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
	}

	void show() {
		SDL_RenderPresent(renderer);
	}

	private:
	SDL_Window *window;
	SDL_Renderer *renderer;

	vector<Toy*> toys;
};

class Ball : public Toy {
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
	
	void on_dt(Playground &pg, uint dt) {
		x0 += vx * dt;
		y0 += vy * dt;
	}


	Ball& at(uint x, uint y) {
		x0 = x; y0 = y;

		return *this;
	}

	Ball& moving(int x, int y) {
		vx = x;
		vy = y;

		return *this;
	}

	private:
	uint red = 0xff, green = 0xff, blue=0, // color
		 x0 = 400, y0 = 300, r = 10; // position & radious
	int  vx = 0, vy = 0; // velocity vector
};

int main(int argc, char **argv) {
	Playground(800,600)
		.with(Ball().at(300,300).moving(1, -1))
		.play();
}
