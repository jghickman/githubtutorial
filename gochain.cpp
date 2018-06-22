
#include "stdafx.h"
#include "isptech/concurrency/channel.hpp"
#include <cstdlib>
#include <iostream>


using namespace Isptech::Concurrency;
using std::atoi;
using std::cerr;
using std::cout;
using std::endl;
using std::exit;
using std::max;


Goroutine
chain(Send_channel<int> left, Receive_channel<int> right)
{
    int n = co_await right.receive();
    co_await left.send(n + 1);
}


void
main(int argc, char* argv[])
{
    int n, result = 0;

    if (argc != 2) {
        cerr << "usage: " << argv[0] << " count\n";
        exit(1);
    }

    n = max(0, atoi(argv[1]));
    if (n > 0) {
        const auto  leftmost    = make_channel<int>();
        auto        right       = leftmost;

        for (int i = 0; i != n; ++i) {
            auto left = right;
            right = make_channel<int>();
            go(chain, left, right);
        }

        right.sync_send(0);
        result = leftmost.sync_receive();
    }

    cout << "total = " << result << endl;
}