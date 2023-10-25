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
        counter (counter const & ) = delete;
        counter (counter && ) = delete;
        counter & operator= (counter const & ) = delete;
        counter & operator= (counter && ) = delete;

        int increment ();
        int count () const;
        void wait_for_value (int v) const;

    private:
        mutable std::mutex mut_;
        mutable std::condition_variable cv_;

        int count_ = 0;
    };

    int counter::increment () {
        std::lock_guard<std::mutex> _{mut_};
        ++count_;
        cv_.notify_one ();
        return count_;
    }

    void counter::wait_for_value (int v) const {
        std::unique_lock<std::mutex> lock{mut_};
        while (count_ < v) {
            cv_.wait (lock);
        }
    }

    int counter::count () const {
        std::unique_lock<std::mutex> lock{mut_};
        return count_;
    }

} // end anonymous namespace

int main (int /*argc*/, char const * /*argv*/[]) {
    using namespace pubsub;

    channel<std::string> chan;

    counter listening_counter;
    counter received_counter;

    std::mutex cout_mut;
    constexpr auto num_subscribers = 3U;
    constexpr auto num_messages = 100U;
    std::array<counter, num_subscribers> per_subscriber_received_counter;

    using subscriber_ptr = std::unique_ptr<subscriber<std::string>>;
    std::vector<std::tuple<subscriber_ptr, std::thread>> subscribers;
    subscribers.reserve (num_subscribers);

    for (auto sub_ctr = 0U; sub_ctr < num_subscribers; ++sub_ctr) {
        subscriber_ptr ptr = chan.new_subscriber ();
        std::thread thread{
            [&](subscriber<std::string> * const sub, int id) {
                listening_counter.increment ();
                while (std::optional<std::string> const message = sub->wait ()) {
                    {
                        std::lock_guard<std::mutex> cout_lock{cout_mut};
                        std::cout << "sub(" << id << "): " << *message << '\n';
                    }
                    received_counter.increment ();
                    per_subscriber_received_counter [id].increment ();
                }
            },
            ptr.get (), sub_ctr};
        subscribers.emplace_back (std::move (ptr), std::move (thread));
    }

    // Wait for our subscribers to get to the point that they're beginning to listen.
    listening_counter.wait_for_value (num_subscribers);

    // Now post some messages to the channel.
    for (auto message_ctr = 0U; message_ctr < num_messages; ++message_ctr) {
        std::this_thread::sleep_for (std::chrono::milliseconds{message_ctr * 10});
        std::ostringstream os;
        os << "message " << message_ctr;
        chan.publish (os.str ());
    }

    received_counter.wait_for_value (num_messages * num_subscribers);

#ifndef NDEBUG
    for (auto & ctr : per_subscriber_received_counter) {
        assert (ctr.count () == num_messages);
    }
#endif

    // Cancel the subscriptions and wait for the threads to complete.
    for (auto & subscriber : subscribers) {
        std::get<0> (subscriber)->cancel ();
        std::get<1> (subscriber).join ();
    }
}
