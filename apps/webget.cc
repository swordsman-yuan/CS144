#include "socket.hh"
// #include "tcp_sponge_socket.hh"
#include "util.hh"

#include <cstdlib>
#include <iostream>

using namespace std;

void get_URL(const string &host, const string &path) {
    // Your code here.

    // You will need to connect to the "http" service on
    // the computer whose name is in the "host" string,
    // then request the URL path given in the "path" string.

    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).

    /*1.establish an address for host, service is http or port 80*/
    Address HostAddress(host, "http");

    /*2.construct a socket for the http connection*/
    TCPSocket Socket1;
    Socket1.connect(HostAddress); // connect to Host Address

    /*3.package a HTTP request datagram*/
    std::string HTTPRequest =   "GET " + path + " HTTP/1.1\r\n" +       // row 1
                                "Host: " + host + "\r\n" +              // row 2
                                "Connection: close\r\n"  +              // row 3
                                "\r\n";                                 // empty line means the end of request

    /*4.send HTTP request datagram, write_all is true*/
    Socket1.write(HTTPRequest, true);

    /*5.read all the response from server*/
    while(!Socket1.eof()) // continue loop until end of file(eof)
    {
        auto ReceivedData = Socket1.read();
        std::cout << ReceivedData;
    }

    /*6.close the socket*/
    Socket1.close();
    // Socket1.wait_until_closed();

    /*debug code is here*/
    // cerr << "Function called: get_URL(" << host << ", " << path << ").\n";
    // cerr << "Warning: get_URL() has not been implemented yet.\n";
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
