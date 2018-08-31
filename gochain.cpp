
#include "stdafx.h"
#include "isptech/concurrency/channel.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>


using namespace Isptech::Concurrency;
using namespace std;


Goroutine
chain(Send_channel<int> left, Receive_channel<int> right)
{
    const int n = co_await right.receive();
    co_await left.send(n + 1);
}


void
main(int argc, char* argv[])
{
    if (argc != 2) {
        cerr << "usage: " << argv[0] << " count\n";
        exit(1);
    }

    int         result  = 0;
    const int   n       = max(atoi(argv[1]), 0);

    if (n > 0) {
        const Channel<int>  leftmost    = make_channel<int>(4);
        Channel<int>        right       = leftmost;

        for (int i = 0; i != n; ++i) {
            const Channel<int> left = right;

            right = make_channel<int>();
            go(chain, left, right);
        }

        right.sync_send(0);
        result = leftmost.sync_receive();
    }

    cout << "total = " << result << endl;
    char c;
    cin >> c;
}
