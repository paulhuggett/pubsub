#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "pubsub.hpp"

int main (int /*argc*/, char const * /*argv*/[]) {
    std::mutex cout_mut;
    using namespace pubsub;

    auto subscription = [&cout_mut] (std::string const & message, int id) {
        std::lock_guard<std::mutex> cout_lock {cout_mut};
        std::cout << "sub(" << id << "): " << message << '\n';
    };

    channel chan;
    std::unique_ptr<subscriber> sub1 = chan.new_subscriber ();
    std::unique_ptr<subscriber> sub2 = chan.new_subscriber ();
    std::thread t1 = sub1->listen (subscription, 1);
    std::thread t2 = sub2->listen (subscription, 2);

    // Initial sleep waits for the two subscribers to (probably) get to the point where they're
    // actually listening.
    for (auto ctr = 0; ctr < 10; ++ctr) {
        std::this_thread::sleep_for (std::chrono::milliseconds{ctr * 25});

        std::ostringstream os;
        os << "message " << ctr;
        chan.publish (os.str ());
    }

    { std::lock_guard<std::mutex> cout_lock {cout_mut}; std::cout << "sleep 2s\n"; }
    std::this_thread::sleep_for (std::chrono::seconds{2});
    //{ std::lock_guard<std::mutex> cout_lock {cout_mut}; std::cout << "awake\n"; }

    chan.unsubscribe (*sub1);
    chan.unsubscribe (*sub2);

    t1.join ();
    t2.join ();
}
