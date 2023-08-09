#include <iostream>
#include "bal.h"

using namespace std;

int main(int argc, char** argv)
{
    bal_initialize();

    bal_socket s = {0};
    bal_sock_create(&s, AF_INET, IPPROTO_TCP, SOCK_STREAM);

    bal_bind(&s, "localhost", "9000");
    bal_listen(&s, 0);

    cout << "Listening; Ctrl+C to exit..." << endl;

    do {
        bal_socket client_sock   = {0};
        bal_sockaddr client_addr = {0};

        bal_accept(&s, &client_sock, &client_addr);

    } while (true);

    bal_finalize();

    return EXIT_SUCCESS;
}
