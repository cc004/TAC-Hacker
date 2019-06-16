#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <ctime>
#include <WinSock2.h>
#include <windows.h>

using namespace std;

const int bufsize = 1024;

class ISocket;
class Socket;
class Packet;

class Exception : public std::exception
{
    char *reason;
public:
    Exception(const char *str)
    {
        reason = new char[strlen(str) + 1];
        strcpy(reason, str);
        std::cout << "Exception thrown : " << reason << std::endl;
    }

    ~Exception()
    {
        delete[] reason;
    }

    const char* what()
    {
        return reason;
    }
};

class IPacket
{
public:
    virtual IPacket& clear() = 0;
    virtual IPacket& setType(unsigned char) = 0;
    virtual IPacket& payload(const char *, size_t) = 0;
    virtual operator const char *() const = 0;
    virtual int recvfrom(ISocket &);
    virtual int getSize() const = 0;
};

class ISocket
{
public:
    virtual void connect(const char *, int) = 0;
    virtual int recv(char *, int) = 0;
    virtual int recv(IPacket&) = 0;
    virtual int send(const char *, int) const = 0;
    virtual int send(const Packet&) const = 0;
};

class Packet : public IPacket
{
    char *buf;
    unsigned short size;
    unsigned char type;
public:
    virtual int recvfrom(ISocket &socket)
    {
        if (buf)
        {
            delete[] buf;
            buf = NULL;
        }
        buf = new char[2];
        socket.recv(buf, 2);
        size = *(unsigned short*)buf;
        delete[] buf;
        socket.recv((char *)&type, 1);
        buf = new char[size];
        socket.recv(&buf[3], size - 3);
        return 123;
    }

    Packet()
    {
        buf = NULL;
        clear();
    }

    virtual IPacket& clear()
    {
        if (buf)
        {
            delete[] buf;
            buf = NULL;
        }
        size = 3;
        type = 0;
        buf = new char[3];
        return *this;
    }

    virtual IPacket& setType(unsigned char t)
    {
        type = t;
        return *this;
    }

    virtual IPacket& payload(const char *buf, size_t size)
    {
        char *t = new char[this->size + size];
        memcpy(t, this->buf, this->size);
        delete[] this->buf;
        memcpy(&t[this->size], buf, size);
        this->buf = t;
        this->size += size;
        return *this;
    }

    template<typename T>
    IPacket& payload(const T& t)
    {
        return payload((const char *)&t, sizeof(T));
    }

    virtual int getSize() const
    {
        return size;
    }

    ~Packet()
    {
        if (buf) delete[] buf;
    }

    virtual operator const char *() const
    {
        *(short *)buf = size;
        buf[2] = type;
        return buf;
    }

    friend std::ostream& operator<<(std::ostream& os, const Packet& p)
    {
        os << "Packet(size=" << p.size << ", type=0x" << std::setbase(16) << (int)p.type << std::setbase(10) << ", payload=";
        for (int i = 3; i < p.size; ++i)
        {
            //cout << p.buf[i];
            if ((unsigned char)p.buf[i] < 16) cout << '0';
            cout << setbase(16) << (int)(unsigned char)p.buf[i] << std::setbase(10) << ' ';
        }
        return os << ")";
    }
};

class Socket : public ISocket
{
    SOCKET socket;
    static bool socketLoaded;
    sockaddr_in sockAddr;
public:
    SOCKET getSocket()
    {
        return socket;
    }

    Socket()
    {  
        if (!socketLoaded) Socket::loadSocket();
        socket = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    }

    Socket(SOCKET socket)
    {  
        if (!socketLoaded) Socket::loadSocket();
        this->socket = socket;
    }

    static void loadSocket()
    {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    static void unloadSocket()
    {  
        WSACleanup();
    }

    virtual void connect(const char *ip, int port)
    {
        memset(&sockAddr, 0, sizeof(sockAddr));
        sockAddr.sin_family = PF_INET;
        sockAddr.sin_addr.s_addr = inet_addr(ip);
        sockAddr.sin_port = htons(port);
        if (::connect(socket, (SOCKADDR *)&sockAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
        {
            char buf[bufsize];
            sprintf(buf, "Connection refused @%s:%d", ip, port);
            throw Exception(buf); 
        }
        else 
        {
            std::cout << "Succussfully connected to " << ip << ':' << port << std::endl;
        }
    }

    virtual int recv(char *buffer, int maxlen)
    {
        int res = ::recv(socket, buffer, maxlen, 0);
        if (res == SOCKET_ERROR) throw Exception("Broken pipe.");
        return res;
    }

    virtual int recv(IPacket& p)
    {
        return p.recvfrom(*this);
    }

    virtual int send(const char *buffer, int length) const
    {
        return ::send(socket, buffer, length, 0);
    }

    virtual int send(const Packet& p) const
    {
        return ::send(socket, p, p.getSize(), 0);
    }

};

bool Socket::socketLoaded = false;

Socket tac, client, server;

void tunnel(Socket server, Socket client)
{
    char c;
    for(;;)
    {
        client.recv((char *)&c, 1);
        server.send((char *)&c, 1);
    }
}

void proxy_server(const char *ip, int port)
{
    SOCKET serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in sockAddr;
    SOCKADDR clntAddr;
    int nSize = sizeof(SOCKADDR);

    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sockAddr.sin_port = htons(7777);
    bind(serversock, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));

    cout << "Awaiting for cheat client connection @127.0.0.1:7777" << endl;
    listen(serversock, 20);
    client = accept(serversock, (SOCKADDR*)&clntAddr, &nSize);

    Packet packet;
    do
    {
        client.recv(packet);
    } while (!packet[2] == 0x01);
    cout << "Cheat client is ready." << endl;

    server.connect(ip, port);
    cout << "Connecting to server @" << ip << ":" << port << endl;
    server.send(packet);

    do
    {
        server.recv(packet);
        if (packet[2] == 0x43)
        {
            cout << "verification packet get =" << packet << endl;
            tac.send(packet);
            tac.recv(packet);
            cout << "respond get from tac =" << packet << endl;
            server.send(packet);
            cout << "packet sent to server." << endl;
            break;
        }
        client.send(packet);
    } while (packet[2] != 0x43);
    
    cout << "starting proxy server..." << endl;
    
    thread stc(tunnel, server, client);
    thread cts(tunnel, client, server);

    cts.join();
    stc.join();
}

int main(int argc, char *argv[])
{
    Socket();
    if (argc < 3) return 1;
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    SOCKET server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in sockAddr;
    SOCKADDR clntAddr;
    int nSize = sizeof(SOCKADDR);

    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.sin_family = PF_INET;
    sockAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sockAddr.sin_port = htons(7778);

    bind(server, (SOCKADDR*)&sockAddr, sizeof(SOCKADDR));
    cout << "Awaiting for tac client connection @127.0.0.1:7778" << endl;
    listen(server, 20);
    tac = accept(server, (SOCKADDR*)&clntAddr, &nSize);

    Packet packet;
    do
    {
        packet.recvfrom(tac);
    } while (!packet[2] == 0x01);
    cout << "Tac client is ready for verification." << endl;

    proxy_server(ip, port);
    Socket::unloadSocket();
}