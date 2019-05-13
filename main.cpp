#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "pubsub.hpp"

int main (int /*argc*/, char const * /*argv*/[]) {
    std::mutex cout_mut;
    using namespace pubsub;

    channel chan;
    std::unique_ptr<subscriber> sub1 = chan.new_subscriber ();
    std::unique_ptr<subscriber> sub2 = chan.new_subscriber ();

    auto get_messages = [&cout_mut](subscriber * const sub, int id) {
        while (std::optional<std::string> message = sub->listen ()) {
            std::lock_guard<std::mutex> cout_lock{cout_mut};
            std::cout << "sub(" << id << "): " << *message << '\n';
        }
    };

    std::thread t1{get_messages, sub1.get (), 1};
    std::thread t2{get_messages, sub2.get (), 2};

    // Initial sleep waits for the two subscribers to (probably) get to the point where they're
    // actually listening.
    for (auto ctr = 0; ctr < 10; ++ctr) {
        std::this_thread::sleep_for (std::chrono::milliseconds{ctr * 25});

        std::ostringstream os;
        os << "message " << ctr;
        chan.publish (os.str ());
    }

    {
        std::lock_guard<std::mutex> cout_lock{cout_mut};
        std::cout << "sleep 2s\n";
    }
    std::this_thread::sleep_for (std::chrono::seconds{2});
    //{ std::lock_guard<std::mutex> cout_lock {cout_mut}; std::cout << "awake\n"; }

    chan.cancel (*sub1);
    chan.cancel (*sub2);

    t1.join ();
    t2.join ();
}
