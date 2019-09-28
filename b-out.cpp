#include <SDL.h>
#include <vector>
#include <cstdlib>
#include <random>

using namespace std;

void fatal() {
	fprintf (stderr, "b-out: %s\n", SDL_GetError());
	SDL_Quit();
   	exit(EXIT_FAILURE);
}

int random (uint min, uint max) {
	std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(min,max);

	return dist(rng);
}

class Playground;

class Toy {
	public:
	virtual void draw(SDL_Renderer *renderer) = 0;
	virtual void on_dt(Playground &pg, uint dt) = 0;
	virtual ~Toy() {}
};

class Collision {
	public:
	Collision(){}

	Collision(uint x, uint y, int vx, int vy)
		: really(true), x(x), y(y), vx(vx), vy(vy) {}

	// really = true if colission happened
	// x, y is the point of colission
	// vx, vy is vector after bounce

	const bool really = false;
	const uint x = 0, y = 0;
	const int vx = 0, vy = 0;
};

class Playground {
    public:
    Playground(uint width, uint height)
			:w(width), h(height) {

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

	Playground& with(vector<Toy*> toys) {
		for(Toy* t: toys)
			with(*t);

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

	Collision obstacle(uint x0, uint y0, uint x1, uint y1, uint r) {
		// This is simple case of straight obstacles as
		// scene boundaries…

		bool left = (int)x1 - r <= 0,
			 top = (int)y1 - r <= 0,
			 right = (int)x1 + r >= w,
			 bottom = (int)y1 + r >= h;

		if(!top && !left && !right && !bottom)
			return Collision();

		int dx = x1 - x0,
			dy = y1 - y0;

		if(dx == 0) {
			if(dy > 0)
				return Collision(x0, h-r, 0, -1);
			else
				return Collision(x0, r, 0, 1);
		}

		if(dy == 0) {
			if(dx > 0)
				return Collision(w-r, y0, -1, 0);
			else
				return Collision(r, y0, 1, 0);
		}

		double xsteps = (dx>0)? (double)(w-x0) / (double) dx
				   			  : (double)x0 / (double) dx,

			   ysteps = (dy>0)? (double)(h-y0) / (double) dy
				   			  : (double)y0 / (double) dy;

		if(fabs(xsteps) < fabs(ysteps))
			return Collision(
				left? r : w -r,
				y0 + xsteps * dy,
				-dx, dy
			);
		else
			return Collision(
				x0 + ysteps * dx,
				top? r : h-r,
				dx, -dy
			);
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

	uint w, h;
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
		uint x1 = x0 + vx * dt;
		uint y1 = y0 + vy * dt;

		Collision c = pg.obstacle(x0, y0, x1, y1, r);
		if(!c.really) {
			x0 = x1;
			y0 = y1;
		} else {
			x0 = c.x;
			y0 = c.y;
			vx = c.vx;
			vy = c.vy;
		}
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

class Box : public Toy {
	public:
	Box() {
		r = random(0,255);
		g = random(0,255);
		b = random(0,255);
	}

	~Box(){}

	Box &at(uint nx, uint ny) {
		x = nx;
		y = ny;

		return *this;
	}

	void draw(SDL_Renderer *renderer) {
		SDL_Rect rect;
		rect.x = x;
		rect.y = y;
		rect.w = w;
		rect.h = h;

		SDL_SetRenderDrawColor(renderer, r, g, b, 255);
		SDL_RenderFillRect(renderer, &rect);
	}

	void on_dt(Playground &pg, uint dt) {
	}

	private:
	uint x = 0, y = 0, w = 50, h = 20;
	uint r, g, b;
};

int main(int argc, char **argv) {
	vector<Toy*> boxes;
	for(uint x = 0; x < 8; x++)
		for(uint y = 0; y < 8; y++){
			Box *b = new Box();

			boxes.push_back(&(b->at(200+50*x, 200+20*y)));
		}

	Playground(800,600)
		.with(Ball().at(400,500).moving(-1, -2))
		.with(boxes)
		.play();

	for(Toy* box : boxes)
		delete box;
}

