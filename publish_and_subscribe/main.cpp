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
    friend class subscriber;
public:
    channel () = default;
    channel (channel const & ) = delete;
    channel (channel && ) = delete;
    channel & operator= (channel const & ) = delete;
    channel & operator= (channel && ) = delete;

    void publish (std::string const & message);

    class subscriber {
        friend class channel;
    public:
        ~subscriber ();

        subscriber (subscriber const & ) = delete;
        subscriber (subscriber && ) = delete;
        subscriber & operator= (subscriber const & ) = delete;
        subscriber & operator= (subscriber && ) = delete;

        void subscribe (std::function<void(std::string const &)> f) {
            owner_->subscribe (this, f);
        }

    private:
        explicit subscriber (channel * c) : owner_ {c} {}

        bool active_ = true;
        std::queue<std::string> queue_;
        channel * owner_;
    };

    std::unique_ptr<subscriber> create_subscriber ();
    void unsubscribe (subscriber * sub);
    void unsubscribe (std::unique_ptr<subscriber> const & sub) { unsubscribe (sub.get ()); }


private:
    void subscribe (subscriber * sub, std::function<void(std::string const &)> f);

    void remove_sub (subscriber * sub);

    std::mutex mut_;
    std::condition_variable cv_;
    std::forward_list<subscriber *> subscribers_;
};

// publish
// ~~~~~~~
void channel::publish (std::string const & message) {
    std::lock_guard<std::mutex> _ {mut_};
    for (auto & sub : subscribers_) {
        sub->queue_.push (message);
    }
    cv_.notify_all ();
}

// create_subscriber
// ~~~~~~~~~~~~~~~~~
std::unique_ptr<channel::subscriber> channel::create_subscriber () {
    std::lock_guard<std::mutex> lock {mut_};
    auto resl = std::unique_ptr<channel::subscriber> {new subscriber (this)};
    subscribers_.emplace_front (resl.get ());
    return resl;
}

// unsubscribe
// ~~~~~~~~~~~
void channel::unsubscribe (subscriber * sub) {
    std::unique_lock<std::mutex> lock {mut_};
    sub->active_ = false;
    cv_.notify_all ();
}

// subscribe
// ~~~~~~~~~
void channel::subscribe (subscriber * const sub, std::function<void(std::string const &)> f) {
    std::unique_lock<std::mutex> lock {mut_};
    sub->active_ = true;
    while (sub->active_) {
        cv_.wait (lock);

        while (sub->active_ && sub->queue_.size () > 0) {
            std::string const message = std::move (sub->queue_.front ());
            sub->queue_.pop ();

            // Don't hold the lock whilst the user callback is called: we can't assume
            // that it will return quickly.
            lock.unlock ();
            f (message);
            lock.lock ();
        }
    }

}

void channel::remove_sub (subscriber * sub) {
    std::lock_guard<std::mutex> _ {mut_};
    subscribers_.remove_if ([sub] (subscriber * s) { return sub == s; });
}

channel::subscriber::~subscriber () {
    owner_->remove_sub (this);
}


int main (int argc, const char * argv[]) {
    std::mutex cout_mut;

    auto subscription = [&cout_mut] (channel & chan, channel::subscriber & sub, int id) {
        pthread_setname_np ("sub");
        sub.subscribe ([id, &cout_mut] (std::string const & message) {
            std::lock_guard<std::mutex> cout_lock {cout_mut};
            std::cout << "sub(" << id << "): " << message << '\n';
        });
    };

    channel chan;
    std::unique_ptr<channel::subscriber> sub1 = chan.create_subscriber ();
    std::unique_ptr<channel::subscriber> sub2 = chan.create_subscriber ();
    std::thread t1 {subscription, std::ref (chan), std::ref (*sub1), 1};
    std::thread t2 {subscription, std::ref (chan), std::ref (*sub2), 2};

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
