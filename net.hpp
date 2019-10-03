#pragma once
#include <string>
#include "SDL_net.h"

using namespace std;

struct NetException {
    NetException();

    string msg;
};

struct RecvTimeout {};

class NetConnection {
    public:
    NetConnection(uint port);
    virtual ~NetConnection();

    void send(string message);
    string receive();

    virtual void establishConnection() = 0;

    protected:
    void commonInit();

    UDPpacket *packet;
    UDPsocket connection;
};

class NetServer : public NetConnection {
    public:
    NetServer();
    ~NetServer();

    void establishConnection();
};

class NetClient : public NetConnection {
    public:
    NetClient(string hostname);
    ~NetClient();
    
    void establishConnection();
};
