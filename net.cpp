#include "net.hpp"
#include <iostream>
#include <cstdlib>

using namespace std;

NetException::NetException() {
    msg = SDLNet_GetError();
}

NetConnection::NetConnection(uint port) {
    if(SDLNet_Init() < 0
       || !(connection = SDLNet_UDP_Open(port))
       || !(packet = SDLNet_AllocPacket(4)))
       throw NetException();
}

NetConnection::~NetConnection() {
    SDLNet_FreePacket(packet);
    SDLNet_UDP_Close(connection);
    SDLNet_Quit();
}

void NetConnection::send(string message) {
    packet->len = 4;
    memcpy(packet->data, message.c_str(), 4);
    if(SDLNet_UDP_Send(connection, -1, packet) == 0)
        throw NetException();
}

string NetConnection::receive() {
    time_t started_at = time(NULL);
    while(SDLNet_UDP_Recv(connection, packet) == 0) {
        time_t now = time(NULL);
        if(now - started_at > 5)
            throw RecvTimeout();
    }
    return string((char *)(packet->data), 4);
}

void NetServer::establishConnection() {
    while(1) {
        try {
            receive();
            return;
        } catch (RecvTimeout e) {}
        cerr << "Waiting for client..." << endl;
    }
}

NetServer::NetServer() : NetConnection(4242) {
    establishConnection();
}

NetServer::~NetServer() {}

NetClient::NetClient(string hostname) : NetConnection(4241) {
    IPaddress addr;
    if(SDLNet_ResolveHost(&addr, hostname.c_str(), 4242))
        throw NetException();
    packet->address = addr;

    establishConnection();
}

void NetClient::establishConnection() {
    SDLNet_UDP_Send(connection, -1, packet);
}

NetClient::~NetClient(){}

#ifdef NET_TEST
int main(int argc, char **argv) {
    bool server = (argc < 2);

    if(server) {
        NetServer srv;
        cout << srv.receive() << endl;
    } else {
        NetClient(argv[1]).send("Hello World!\n");
    }
}
#endif
