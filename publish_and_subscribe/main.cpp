//
//  main.cpp
//  publish_and_subscribe
//
//  Created by Paul Bowen-Huggett on 10/05/2019.
//  Copyright © 2019 Paul Bowen-Huggett. All rights reserved.
//

#include <functional>
#include <iostream>
#include <condition_variable>
#include <thread>
#include <tuple>
#include <string>
#include <sstream>
#include <vector>
#include <queue>
#include <mutex>
#include <map>
#include <chrono>
#include <forward_list>

class channel {
public:
    channel () = default;
    channel (channel const & ) = delete;
    channel (channel && ) = delete;
    channel & operator= (channel const & ) = delete;
    channel & operator= (channel && ) = delete;

    void publish (std::string const & message);

    struct subscriber {
        friend class channel;
    public:
        explicit subscriber (channel * c) : owner {c} {}

        subscriber (subscriber const & ) = delete;
        subscriber (subscriber && ) = delete;
        subscriber & operator= (subscriber const & ) = delete;
        subscriber & operator= (subscriber && ) = delete;

        void subscribe (std::function<void(std::string const &)> f) {
            owner->subscribe (this, f);
        }

    private:
        bool active = true;
        std::queue<std::string> queue;
        channel * owner;
    };


    subscriber * create_subscriber ();
    void unsubscribe (subscriber * sub);

    void subscribe (subscriber * sub, std::function<void(std::string const &)> f);

private:
    std::mutex mut_;
    std::condition_variable cv_;
    std::forward_list<subscriber> subscribers_;
};

// publish
// ~~~~~~~
void channel::publish (std::string const & message) {
    std::lock_guard<std::mutex> _ {mut_};
    for (auto & sub : subscribers_) {
        sub.queue.push (message);
    }
    cv_.notify_all ();
}

// create_subscriber
// ~~~~~~~~~~~~~~~~~
channel::subscriber * channel::create_subscriber () {
    std::lock_guard<std::mutex> lock {mut_};
    subscribers_.emplace_front (this);
    return &subscribers_.front ();
}

// unsubscribe
// ~~~~~~~~~~~
void channel::unsubscribe (subscriber * sub) {
    std::unique_lock<std::mutex> lock {mut_};
    sub->active = false;
    cv_.notify_all ();
}

// subscribe
// ~~~~~~~~~
void channel::subscribe (subscriber * const sub, std::function<void(std::string const &)> f) {
    std::unique_lock<std::mutex> lock {mut_};
    while (sub->active) {
        cv_.wait (lock);

        while (sub->active && sub->queue.size () > 0) {
            std::string const message = std::move (sub->queue.front ());
            sub->queue.pop ();

            // Don't hold the lock whilst the user callback is called: we can't assume
            // that it will return quickly.
            lock.unlock ();
            f (message);
            lock.lock ();
        }
    }

    subscribers_.remove_if ([sub] (subscriber & s) { return sub == &s; });
}



int main (int argc, const char * argv[]) {
    std::mutex cout_mut;

    auto subscription = [&cout_mut] (channel & chan, channel::subscriber * const sub, int id) {
        pthread_setname_np ("sub");
        sub->subscribe ([id, &cout_mut] (std::string const & message) {
            std::lock_guard<std::mutex> cout_lock {cout_mut};
            std::cout << "sub(" << id << "): " << message << '\n';
        });
    };

    channel chan;
    channel::subscriber * const sub1 = chan.create_subscriber ();
    channel::subscriber * const sub2 = chan.create_subscriber ();
    std::thread t1 {subscription, std::ref (chan), sub1, 1};
    std::thread t2 {subscription, std::ref (chan), sub2, 2};

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

    chan.unsubscribe (sub1);
    chan.unsubscribe (sub2);

    t1.join ();
    t2.join ();
}
