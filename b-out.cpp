#include <SDL.h>
#include <vector>
#include <list>
#include <map>
#include <cstdlib>
#include <random>
#include <iostream>

#include "net.hpp"

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

void write16(void *buff, uint n) {
    if(n & 0xffff0000)
        cerr << "write16: value exeeds 16bits" << endl;

    ((char *) buff)[0] = (char)(n & 0xff);
    ((char *) buff)[1] = (char)((n >> 8) & 0xff);
}

uint read16(const void *buff) {
    return (((uint)((char*) buff)[0]) & 0xff)
        + ((((uint)((char*) buff)[1]) << 8) & 0xff00);
}

struct WrongFormat {};


/* Point, Line, Segment, Mov. Classes to be considered in
 * sense of analytical geometry. */

struct Point {
    Point(): x(0), y(0) {}
    Point(uint x, uint y): x(x), y(y) {}
    Point(string bin) {
        if(bin.size() != 4)
            throw WrongFormat();

        x = read16(bin.data());
        y = read16(bin.data()+2);
    };

    uint dist(Point b) {
        int dx = b.x - x,
            dy = b.y - y;

        return floor(sqrt(dx*dx + dy*dy));
    }

    string bin() {
        char dst[4];
        write16(dst, x);
        write16(dst+2, y);

        return string(dst, 4);
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
                    && candidate->x <= max(s.a.x, s.b.x)
                    && candidate->y >= min(a.y, b.y)
                    && candidate->y <= max(a.y, b.y)
                    && candidate->y >= min(s.a.y, s.b.y)
                    && candidate->y <= max(s.a.y, s.b.y))

            return candidate;
        else
            return optional<Point>();
    }

    optional<Point> closePoint(Segment s, uint distance) {
        Line base(a,b);
        Point p1 = *(base.intersection(base.perpendicular(s.b)));

        double d;
        if(a.x == b.x && p1.y >= min(a.y, b.y)
                      && p1.y <= max(a.y, b.y))
            d = p1.dist(s.b);
        else if(a.y == b.y && p1.x >= min(a.x, b.x)
                      && p1.x <= max(a.x, b.x))
            d = p1.dist(s.b);
        else
            d = min(a.dist(s.b), b.dist(s.b));

        if(d <= distance)
            return Point((s.a.x+s.b.x)/2,
                        (s.a.y+s.b.y)/2);

        return optional<Point>();
    } 

    Segment moved(Mov m) {
        return Segment(m.apply(a), m.apply(b));
    }

    Point a, b;
};

/* Game mechanics.
 * Playground is a central class, it defines comminication
 * with graphics API, it coordinates all elements of the
 * game. Playground.play() function contains program's
 * main event loop. */

class Playground;


/* Toy is an interface for all things displayed on the screen.
 *  draw() function called by playground to draw object on the
 *         screen.
 *
 *  timePassed() function called by playground to notify object
 *               that it should update its state. For example,
 *               apply its movement. dt means how much time
 *               passed, usually 1 unit.
 *
 *  collision() function called by playground to notify that
 *              there was collision with it. Currently collision()
 *              may happen only with a ball and ball is not notified,
 *              because it asks playground if it can move to given
 *              point, so that it knows.
 *
 *  destroyed() function called by playground to ask if object should
 *              be removed. Usually a box after certain number of
 *              collisions.
 *
 *  boundaries() function called by playground to get segments that
 *               represent object boundaries to be used in collision
 *               detection.
 */
class Toy {
    public:
    virtual void draw(SDL_Renderer *renderer) = 0;
    virtual void timePassed(Playground &pg, uint dt) = 0;
    virtual ~Toy() {}
    virtual void collision() {}
    virtual bool destroyed() {return false;}

    vector<Segment> &boundaries() {
        return bounds;
    }

    protected:
    vector<Segment> bounds;
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

class KeyListener {
    public:
    virtual void keyPress(int action) = 0;
};

struct KeyBinding {
    KeyBinding(){}
    KeyBinding(KeyListener *listener, int actionId)
        : listener(listener), action(actionId) {}

    void trigger() {
        listener->keyPress(action);
    }

    KeyListener *listener;
    int         action;
};

/* Interface that represents a player.
 * It's common for local and remote players. It's
 * responsible to place bat and a ball in the playground.
 * It also handles network communication if applicable.
 *  initPlayer() called by playground asking to add objects
 *               to it.
 *  timePassed() called by playground each iteration of main
 *               loop. It passes position of opponents bat and
 *               asks position of this player's bat. It is an
 *               interface for network communication. It is
 *               called only if wantsUpdates() returns true.
 */
class Player {
    public:
    virtual ~Player() {};
    virtual void initPlayer(Playground &pg) = 0;
    virtual Point timePassed(Point other) = 0;
    virtual bool wantsUpdates() {return false;}
    virtual Point getPos() = 0;
    virtual void setPos(Point pos) = 0;
};

// Exception thrown when trying to add third player.
struct TooManyPlayers {};

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
        for(Player *p : players)
            delete p;

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

    Playground& with(optional<Player*> player) {
        if(player) {
            if(players.size() == 2)
                throw TooManyPlayers();
            
            (*player)->initPlayer(*this);
            players.push_back(*player);
        }

        return *this;
    }

    Playground& with(vector<Toy*> toys) {
        for(Toy* t: toys)
            with(*t);

        return *this;
    }

    void play() {
        bool done = false;
        bool pause = false;
        uint last_time = SDL_GetTicks();
        while(!done) {
            SDL_Event e;
            while(SDL_PollEvent(&e) != 0) {
                if(e.type == SDL_KEYDOWN) {
                    auto b = keyBindings.find(e.key.keysym.sym);
                    if (b != keyBindings.end())
                        downKeys[b->first] = b->second;
                    else if (e.key.keysym.sym == SDLK_q)
                        done = true;
                    else if (e.key.keysym.sym == SDLK_ESCAPE)
                        pause = !pause;
                }

                if(e.type == SDL_KEYUP) {
                    auto b = downKeys.find(e.key.keysym.sym);
                    if (b != downKeys.end()) {
                        downKeys.erase(b);
                    }
                }
            }

            for(auto i = downKeys.begin(); i != downKeys.end(); i++) {
                i->second.trigger();
            }

            if (!pause) {
                newFrame();
                if(players.size() == 2) {
                    Player *a = NULL, *b = NULL;
                    if(players.front()->wantsUpdates()) {
                        a = players.front();
                        b = players.back();
                    } else if (players.back()->wantsUpdates()) {
                        a = players.back();
                        b = players.front();
                    }

                    if(a != NULL) {
                        a->setPos(a->timePassed(b->getPos()));
                    }
                }

                for(Toy *toy : toys) {
                    toy->timePassed(*this, 1);
                    toy->draw(renderer);
                }
            }
            show();

            while(SDL_GetTicks() < last_time + 17); // aim at 60fps
            last_time = SDL_GetTicks();
        }
    }

    /* Collision detecting function. Route is a vector that represents
     * movement would happend during current portion of time. r represents
     * radious of the calling object. */
    Collision obstacle(Segment route, uint r) {
        optional<Point> intersection;
        Segment *is = NULL;
        Toy *toy = NULL;

        for(Segment &s : boundaries) {
            optional<Point> i = s.closePoint(route, r);
            if(i && (!intersection
                     || intersection->dist(route.a) > i->dist(route.a))) {
                intersection = i;
                is = &s;
            }
        }

        for(Toy *t : toys) {
            for(Segment &s : t->boundaries()) {
                optional<Point> i = s.closePoint(route, r);
                if(i && (!intersection
                            || intersection->dist(route.a) > i->dist(route.a))) {
                    intersection = i;
                    is = &s;
                    toy = t;
                }
            }
        }

        if (toy) {
            toy->collision();
            if(toy->destroyed())
                toys.remove(toy);
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

    Playground& withKey(int keysym, KeyBinding binding) {
        keyBindings[keysym] = binding;

        return *this;
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

    list<Player*> players;
    map<int,KeyBinding> downKeys;
    map<int,KeyBinding> keyBindings;
    vector<Segment> boundaries;
    list<Toy*> toys;
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
    
    void timePassed(Playground &pg, uint dt) {
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

    enum Direction {
        up = -1, down = 1
    };

    private:
    uint    red = 0xff, green = 0xff, blue=0, r=10;
    Point   pos = Point(400,300);
    Mov     velocity = Mov(0,0);
};

class Box : public Toy {
    public:
    Box() {
        r = random(10,255);
        g = random(10,255);
        b = random(10,255);

        refresh();
    }

    ~Box(){}

    Box &at(Point p) {
        pos = p;
        refresh();

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

    void collision() {
        hits++;
        r = random(10,255);
        g = random(10,255);
        b = random(10,255);
    }

    bool destroyed() { return hits >= 2; }
    
    void timePassed(Playground &pg, uint dt) {
    }

    private:
    
    void refresh() {
        bounds.clear();
        Point a(pos.x,      pos.y),
              b(pos.x + w,  pos.y),
              c(pos.x + w,  pos.y + h),
              d(pos.x,      pos.y + h);

        bounds.push_back(Segment(a, b));
        bounds.push_back(Segment(b, c));
        bounds.push_back(Segment(c, d));
        bounds.push_back(Segment(d, a));
    }

    Point   pos = Point(0,0);
    uint    w = 50, h = 20;
    uint    r, g, b;
    uint    hits = 0;
};

class Bat : public Toy, public KeyListener {
    public:
    Bat() {
        refresh();
    }

    ~Bat() {}

    void timePassed(Playground &pg, uint dt) {}
    void draw(SDL_Renderer *renderer) {
        SDL_Rect rect;
        rect.x = pos.x;
        rect.y = pos.y;
        rect.w = w;
        rect.h = h;

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderFillRect(renderer, &rect);
    }

    Bat &at(Point p) {
        pos = p;
        refresh();

        return *this;
    }

    void refresh() {
        bounds.clear();
        Point a(pos.x,      pos.y),
              b(pos.x + w,  pos.y),
              c(pos.x + w,  pos.y + h),
              d(pos.x,      pos.y + h);

        bounds.push_back(Segment(a, b));
        bounds.push_back(Segment(b, c));
        bounds.push_back(Segment(c, d));
        bounds.push_back(Segment(d, a));
    }

    void keyPress(int action) {
        switch(action) {
            case moveLeft:
                pos.x -= 7;
                break;
            case moveRight:
                pos.x += 7;
        }

        refresh();
    }

    Point getPos() { return pos; }

    enum Actions {
        moveLeft, moveRight
    };

    private:
    Point   pos = Point(350,750);
    uint    w = 100, h=10;
    uint    r = 150, g = 150, b = 150;
};

Mov initialBallMovement(Ball::Direction direction) {
    int dx = 3;
    int dy = direction * (6 - abs(dx));
    return Mov(dx, dy);
}

class GenericPlayer : public Player {
    public:
    GenericPlayer(Point position, Ball::Direction direction) {
        ball = Ball().at(Mov(0, direction * 50).apply(position))
                     .moving(initialBallMovement(direction));
        bat = Bat().at(position);
    }
    ~GenericPlayer(){}

    Point getPos() { return bat.getPos(); };
    void setPos(Point pos) { bat.at(pos); }

    protected:
    Bat bat;
    Ball ball;
};

class LocalPlayer : public GenericPlayer {
    public:
    LocalPlayer(Point p, Ball::Direction d) : GenericPlayer(p, d) {}
    ~LocalPlayer() {};

    LocalPlayer *withKeys(int moveLeft, int moveRight) {
        lKey = moveLeft;
        rKey = moveRight;

        return this;
    }

    void initPlayer(Playground &pg) {
        pg.with(ball).with(bat)
            .withKey(lKey, KeyBinding(&bat, (int)Bat::moveLeft))
            .withKey(rKey, KeyBinding(&bat, (int)Bat::moveRight));
    }

    Point timePassed(Point other) {
        return Point(0,0);
    }

    private:
    int lKey, rKey;
};

class RemotePlayer : public GenericPlayer {
    public:
    RemotePlayer(Point position, Ball::Direction direction)
        : GenericPlayer (position, direction) {}
    ~RemotePlayer() {}

    bool wantsUpdates() {return true;}

    void initPlayer(Playground &pg) {
        pg.with(ball).with(bat);
    }
};

class GuestRemote : public RemotePlayer {
    public:
    GuestRemote(NetServer *conn, Point position, Ball::Direction direction)
        : RemotePlayer (position, direction), conn(conn) {}
    ~GuestRemote() { delete conn; }

    Point timePassed(Point other) {
        conn->send(other.bin());
        return Point(conn->receive());
    }

    private:
    NetServer *conn;
};

class HostRemote : public RemotePlayer {
    public:
    HostRemote(NetClient *conn, Point position, Ball::Direction direction)
       : RemotePlayer(position, direction), conn(conn) {} 
    ~HostRemote() { delete conn; }

    Point timePassed(Point other) {
        Point resp = Point(conn->receive());
        conn->send(other.bin());
        return resp;
    }

    private:
    NetClient *conn;
};

enum Mode {
    server, client, localmulti, single
} mode = single;

/* This function decides if there should be second player and
 * constructs its object depending on game mode. */
optional<Player*> playerForMode(Mode m, char *arg) {
    switch(m) {
        case server:
            cout << "Waiting for second player…" << endl;
            return optional<Player*>(
                    new GuestRemote(
                            new NetServer(),
                            Point(350, 50),
                            Ball::down
                    )
            );
        case localmulti:
            return optional<Player*>(
                    (new LocalPlayer(Point(350, 50), Ball::down))
                        ->withKeys(SDLK_a, SDLK_d)
            );
        case client:
            return optional<Player*>(
                    new HostRemote(
                            new NetClient(string(arg)),
                            Point(350, 550),
                            Ball::up
                    )
            );
        default:
            return optional<Player*>();
    }
}

int main(int argc, char **argv) {
    if(argc > 1) {
       if(strcmp(argv[1], "--server") == 0)
           mode = server;
       else if(strcmp(argv[1], "--localmulti") == 0)
           mode = localmulti;
       else
           mode = client;
    }

    vector<Toy*> boxes;
    for(uint x = 0; x < 8; x++)
        for(uint y = 0; y < 8; y++){
            Box *b = new Box();

            boxes.push_back(&(b->at(Point(200+50*x, 200+20*y))));
        }

    Playground playground(800,600);
    playground.with(boxes)
              .with(((mode == client)?
                          new LocalPlayer(Point(350,50), Ball::down)
                          :new LocalPlayer(Point(350,550), Ball::up))
                        ->withKeys(SDLK_LEFT, SDLK_RIGHT))
              .with(playerForMode(mode, argv[1]))
              .play();

    for(Toy* box : boxes)
        delete box;
}

