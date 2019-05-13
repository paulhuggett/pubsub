#include <array>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "pubsub.hpp"

namespace {

    class counter {
    public:
        counter () = default;

        int increment (int by = 1);
        void wait_for_value (int v);

    private:
        std::mutex mut;
        std::condition_variable cv_;
        int count_ = 0;
    };

    int counter::increment (int by) {
        std::lock_guard<std::mutex> _ {mut};
        count_ += by;
        int result = count_;
        cv_.notify_one ();
        return result;
    }

    void counter::wait_for_value (int v) {
        std::unique_lock<std::mutex> lock{mut};
        while (count_ < v) {
            cv_.wait (lock);
        }
    }

} // end anonymous namespace

int main (int /*argc*/, char const * /*argv*/[]) {
    std::mutex cout_mut;
    using namespace pubsub;

    channel chan;
    counter listening_counter;
    counter received_counter;
    constexpr auto num_subscribers = 2U;
    constexpr auto num_messages = 10U;

    auto get_messages = [&cout_mut, &listening_counter, &received_counter](subscriber * const sub, int id) {
        listening_counter.increment();
        while (std::optional<std::string> message = sub->listen ()) {
            {
                std::lock_guard<std::mutex> cout_lock{cout_mut};
                std::cout << "sub(" << id << "): " << *message << '\n';
            }
            received_counter.increment();
        }
    };

    using subscriber_ptr = std::unique_ptr<subscriber>;
    std::vector <std::tuple <std::unique_ptr<subscriber>, std::thread>> subscribers;
    subscribers.reserve (num_subscribers);
    for (auto ctr = 0; ctr < num_subscribers; ++ctr) {
        subscriber_ptr ptr = chan.new_subscriber ();
        std::thread thread{get_messages, ptr.get (), 1};
        subscribers.emplace_back (std::move (ptr), std::move (thread));
    }

    // Wait for our subscribers to get to the point that they're beginning to listen.
    listening_counter.wait_for_value (num_subscribers);

    // Now post some messages to the channel.
    for (auto ctr = 0U; ctr < num_messages; ++ctr) {
        std::this_thread::sleep_for (std::chrono::milliseconds{ctr * 25});
        std::ostringstream os;
        os << "message " << ctr;
        chan.publish (os.str ());
    }

    received_counter.wait_for_value (num_messages * num_subscribers);

    // Cancel the subscriptions and wait for the threads to complete.
    for (auto & subscriber : subscribers) {
        std::get<0> (subscriber)->cancel ();
        std::get<1> (subscriber).join ();
    }
}
