#include <SDL.h>
#include <vector>
#include <cstdlib>
#include <random>
#include <iostream>

using namespace std;

class bad_optional : public exception {};

// optional is part of C++17, but not in my compiler :)
template<class T>
class optional {
    public:
    optional(): present(false) {}
    optional(T value): val(value), present(true){}

    operator bool() {return present;}
    T operator* () {
        if(!present)
            throw bad_optional();
        return val;
    }

    T* operator-> () {
        if(!present)
            throw bad_optional();
        return &val;
    }

    private:

    T val;
    bool present;
};

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

struct Point {
    Point(): x(0), y(0) {}
    Point(uint x, uint y): x(x), y(y) {}

    uint dist(Point b) {
        int dx = b.x - x,
            dy = b.y - y;

        return floor(sqrt(dx*dx + dy*dy));
    }

    uint x, y;
};

struct Mov {
    Mov(int dx, int dy): dx(dx), dy(dy) {}

    Point apply(Point p) {
        return Point(p.x + dx, p.y + dy);
    }

    int dx, dy;
};

struct Segment;

class Line {
    public:
    Line(Point a, Point b) {
        angle = (double)((int)b.y - (int)a.y) / ((int)b.x - (int)a.x);

        if(isinf(angle))
            x = a.x; 
        else
            y0 = a.y - angle*a.x;
    }

    optional<Point> intersection(Line b) {
        if(angle == b.angle)
            return optional<Point>();

        if(isinf(angle)) 
            return optional<Point>(Point(x, x*b.angle + b.y0));

        if(isinf(b.angle))
            return optional<Point>(Point(b.x, b.x*angle + y0));
            

        // y = a0 * x + b0
        // y = a1 * x + b1
        // thus
        // a0 * x + b0 = a1 * x + b1
        // a0 * x - a1 * x = b1 - b0
        // (a0 - a1)*x = b1 - b0
        // x = (b1 - b0) / (a0 - a1)
        // having x, y is easy

        double x = (b.y0 - y0) / (angle - b.angle);
        return optional<Point>(Point(x, angle * x + y0));
    }

    Line perpendicular(Point p) {
        double a = -(1/angle);
        if(isinf(a))
            return Line(a, p.x);
        else
            return Line(a, p.y - a * p.x);
    }

    uint dist(Point p) {
        Point p2 = *(intersection(perpendicular(p)));
        return p.dist(p2);
    }

    private:
    Line(double a, double b) {
        angle = a;
        if(isinf(angle))
            x = b;
        else
            y0 = b;
    }

    double angle, y0, x;
};

struct Segment {
    Segment(Point a, Point b): a(a), b(b) {}

    optional<Point> intersection(Segment s) {
        optional<Point> candidate = Line(a, b).intersection(Line(s.a, s.b));
        if(candidate && candidate->x >= min(a.x, b.x)
                    && candidate->x <= max(a.x,b.x)
                    && candidate->x >= min(s.a.x, s.b.x)
                    && candidate->x <= max(s.a.x, s.b.x))
            return candidate;
        else
            return optional<Point>();
    }

    optional<Point> closePoint(Segment s, uint distance) {
        Line base(a,b);
        if(base.dist(s.b) <= distance)
            return Point((s.a.x+s.b.x)/2,
                        (s.a.y+s.b.y)/2);

        return optional<Point>();
    } 

    Segment moved(Mov m) {
        return Segment(m.apply(a), m.apply(b));
    }

    Point a, b;
};

class Playground;

class Toy {
	public:
	virtual void draw(SDL_Renderer *renderer) = 0;
	virtual void time_passed(Playground &pg, uint dt) = 0;
	virtual ~Toy() {}
};

struct Collision {
	public:
	Collision(){}

	Collision(Point where, Mov continuation)
		: really(true), where(where), continuation(continuation) {}

	bool really = false;
	Point where = Point(0,0);
	Mov continuation = Mov(0,0);
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

        Point a = Point(0, 0),
              b = Point(w, 0),
              c = Point(w, h),
              d = Point(0, h);

        boundaries.push_back(Segment(a, b));
        boundaries.push_back(Segment(b, c));
        boundaries.push_back(Segment(c, d));
        boundaries.push_back(Segment(d, a));
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
				toy->time_passed(*this, 1);
				toy->draw(renderer);
			}
			show();
		}
	}

	Collision obstacle(Segment route, uint r) {
        optional<Point> intersection;
        Segment *is;

        for(Segment &s : boundaries) {
            optional<Point> i = s.closePoint(route, r);
            if(i && (!intersection
                     || intersection->dist(route.a) > i->dist(route.a))) {
                intersection = i;
                is = &s;
            }
        }

        if(intersection)
            return Collision(
                    *intersection,
                    (is->a.x == is->b.x)?
                        Mov(route.a.x - route.b.x,
                            route.b.y - route.a.y)
                        :Mov(route.b.x - route.a.x,
                            route.a.y - route.b.y)
            );

        return Collision();
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

    vector<Segment> boundaries;
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
				pos.x - dx, pos.y - dy, pos.x + dx, pos.y - dy
			);
			SDL_RenderDrawLine(renderer,
				pos.x - dx, pos.y + dy, pos.x + dx, pos.y + dy
			);
		}

		SDL_RenderDrawLine(renderer, pos.x - r, pos.y, pos.x + r, pos.y);
	}
	
	void time_passed(Playground &pg, uint dt) {
        Point dest = velocity.apply(pos);

		Collision c = pg.obstacle(Segment(pos, dest), r);
		if(!c.really) {
            pos = dest;
		} else {
            pos = c.continuation.apply(c.where);
            velocity = c.continuation;
		}
	}


	Ball& at(Point p) {
		pos = p;

		return *this;
	}

	Ball& moving(Mov m) {
        velocity = m;

		return *this;
	}

	private:
	uint    red = 0xff, green = 0xff, blue=0, r=10;
    Point   pos = Point(400,300);
    Mov     velocity = Mov(0,0);
};

class Box : public Toy {
	public:
	Box() {
		r = random(0,255);
		g = random(0,255);
		b = random(0,255);
	}

	~Box(){}

	Box &at(Point p) {
        pos = p;

		return *this;
	}

	void draw(SDL_Renderer *renderer) {
		SDL_Rect rect;
		rect.x = pos.x;
		rect.y = pos.y;
		rect.w = w;
		rect.h = h;

		SDL_SetRenderDrawColor(renderer, r, g, b, 255);
		SDL_RenderFillRect(renderer, &rect);
	}

	void time_passed(Playground &pg, uint dt) {
	}

	private:
    Point   pos = Point(0,0);
	uint    w = 50, h = 20;
	uint    r, g, b;
};

int main(int argc, char **argv) {
	vector<Toy*> boxes;
	for(uint x = 0; x < 8; x++)
		for(uint y = 0; y < 8; y++){
			Box *b = new Box();

			boxes.push_back(&(b->at(Point(200+50*x, 200+20*y))));
		}

	Playground(800,600)
		.with(Ball().at(Point(400,500)).moving(Mov(-1, -2)))
		.with(boxes)
		.play();

	for(Toy* box : boxes)
		delete box;
}

