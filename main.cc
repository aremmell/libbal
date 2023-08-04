#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <string>
#include "bal.h"

using namespace std;

void async_cb(const balst* st, int event)
{
    //printf("async_cb: event: %08X\n", event);

    if (event & BAL_E_CONNECT) {
        printf("connected to remote host!\n");
        sleep(1);
    }

    if (event & BAL_E_CONNFAIL) {
        printf("ERROR: failed to connect to remote host! err: %d\n",
            bal_geterror(st));
        return;
    }

    if (event & BAL_E_EXCEPTION) {
        printf("ERROR: got exception condition! err: %d\n",
            bal_geterror(st));
        return;
    }

    if (event & BAL_E_READ) {
        printf("socket is readable, reading!\n");
        char buf[2048] = {0};

        while (0 < bal_recv(st, (void*)&buf[0], 2047, 0)) {
            printf("recv: %s\n", buf);
        }
    }

    static bool sent_get = false;
    if (event & BAL_E_WRITE) {
        if (!sent_get) {
            printf("socket is writable, writing!\n");
            const char* getreq = "GET / HTTP/1.1\nHost: rml.dev\nUser-Agent: aremmell\nAccept: */*\n\n";
            int sent = bal_send(st, (const void*)getreq, strlen(getreq), 0);
            if (0 < sent)
                sent_get = true;
            printf("send() returns %d\n", sent);
        }
        sleep(1);
    }

    if (event & BAL_E_CLOSE) {
        printf("connection closed by peer!\n");
    }
}

int main(int argc, char** argv)
{
    balst bs = {0};
    bal_addrlist al = {0};
    bal_addrstrings as = {0};
    const bal_sockaddr* psa = NULL;

    if (BAL_TRUE != bal_initialize()) {
        perror("bal_initialize");
        goto _cleanup;
    }

    // Let's create a socket, and play around...
    if (BAL_TRUE != bal_sock_create(&bs, AF_INET, IPPROTO_TCP, SOCK_STREAM)) {
        perror("bal_sock_create");
        goto _cleanup;
    }

    printf("created IPv4/TCP socket (descriptor: %d)\n", bs.sd);

    /*if (BAL_TRUE != bal_setiomode(&bs, FIOASYNC)) {
        perror("bal_setiomode");
        goto _cleanup;
    }*/

    if (BAL_TRUE != bal_connect(&bs, "rml.dev", "80")) {
        perror("bal_connect");
        goto _cleanup;
    }

    if (BAL_TRUE != bal_asyncselect(&bs, &async_cb, BAL_E_ALL)) {
        perror("bal_asyncselect");
        goto _cleanup;
    }

    printf("Now asynchronously receiving events; press ctrl+C to exit\n");
    sleep(5000);

/*     if (BAL_TRUE != bal_bind(&bs, "127.0.0.1", "1337")) {
        perror("bal_bindaddrany");
        goto _cleanup;
    } */

/*     // Test get local host strings
    if (BAL_TRUE != bal_getlocalhoststrings(&bs, 1, &as)) {
        perror("bal_getlocalhoststrings");
    } else {
        // Got em. Enumerate them...
        printf("Local host information:\n\thostname: '%s'\n\tIP: '%s'\n\ttype: '%s'\n\tport: '%s'\n",
            as.host, as.ip, as.type, as.port);
    } */
/*
    if (BAL_TRUE != bal_getlocalhostaddr(&bs, &sa)) {
        perror("bal_getlocalhostaddr");
        goto _cleanup;
    } */

/*     // resolve
    if (BAL_TRUE != bal_resolvehost("ulfberht.local", &al)) {
        perror("bal_resolvehost");
        goto _cleanup;
    }


    do {
        psa = bal_enumaddrlist(&al);
        if (psa) {
            bal_getaddrstrings(psa, 1, &as);
            printf("addr:\n\thostname: '%s'\n\tIP: '%s'\n\ttype: '%s'\n\tport: '%s'\n",
            as.host, as.ip, as.type, as.port);
        }
    } while (psa);

    bal_freeaddrlist(&al); */

_cleanup:
    printf("cleaning up...\n");

    if (BAL_TRUE != bal_close(&bs))
        printf("WARNING: failed to close socket!\n");

    if (BAL_TRUE != bal_finalize()) {
        perror("bal_finalize");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
