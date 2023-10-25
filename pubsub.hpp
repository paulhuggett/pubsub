#ifndef PUBSUB_HPP
#define PUBSUB_HPP

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_set>

namespace pubsub {

    template <typename Message>
    class channel;

    //*          _               _ _              *
    //*  ____  _| |__ ___ __ _ _(_) |__  ___ _ _  *
    //* (_-< || | '_ (_-</ _| '_| | '_ \/ -_) '_| *
    //* /__/\_,_|_.__/__/\__|_| |_|_.__/\___|_|   *
    //*                                           *
    /// An instance of the subscriber class represents a subscription to messages published on an
    /// associated owning channel.
    template <typename Message>
    class subscriber {
        friend class channel<Message>;
    public:
        explicit constexpr subscriber (channel<Message> * const c) noexcept
                : owner_{c} {
            assert (c != nullptr);
        }
        virtual ~subscriber () noexcept;

        // No copying or assignment.
        subscriber (subscriber const &) = delete;
        subscriber (subscriber &&) noexcept = delete;
        subscriber & operator= (subscriber const &) = delete;
        subscriber & operator= (subscriber &&) noexcept = delete;

        /// Blocks waiting for a message to be published on the owning channel of for the
        /// subscription to be cancelled.
        ///
        /// \returns An optional holding a message published to the owning channel or with no value
        /// indicating that the subscription has been cancelled.
        std::optional<Message> wait ();

        /// \param rel_time A duration representing the maximum time to spend waiting. Note that rel_time must be small enough not to overflow when added to std::chrono::steady_clock::now().
        template <typename Rep, typename Period>
        std::optional<Message> wait_for (std::chrono::duration<Rep, Period> const & rel_time);

        /// \param timeout_time	-	An object of type std::chrono::time_point representing the time when to stop waiting.
        template <typename Clock, typename Duration>
        std::optional<Message> wait_until (std::chrono::time_point<Clock, Duration> const & timeout_time);

        /// Cancels a subscription.
        ///
        /// The subscription is marked as inactive. If waiting it is woken up.
        void cancel ();
        [[nodiscard]] constexpr bool active () const noexcept { return active_; }

        /// \returns A reference to the owning channel.
        [[nodiscard]] channel<Message> & owner () noexcept { return *owner_; }
        /// \returns A reference to the owning channel.
        [[nodiscard]] channel<Message> const & owner () const noexcept { return *owner_; }

    private:
        /// The queue of published messages waiting to be delivered to a listening subscriber.
        ///
        /// \note This is a queue of strings. If there are multiple subscribers to this channel then
        /// the strings will be duplicated in each which could be very inefficient. An alternative
        /// would be to store shared_ptr<string>. For the moment I'm leaving it like this on the
        /// assumption that there will typically be just a single subscriber.
        std::queue<Message> queue_;

        /// The channel with which this subscription is associated.
        channel<Message> * const owner_;

        /// Should this subscriber continue to listen to messages?
        bool active_ = true;
    };

    //*     _                       _  *
    //*  __| |_  __ _ _ _  _ _  ___| | *
    //* / _| ' \/ _` | ' \| ' \/ -_) | *
    //* \__|_||_\__,_|_||_|_||_\___|_| *
    //*                                *
    /// Messages can be written ("published") to a channel; there can be multiple "subscribers"
    /// which will all receive notification of published messages.
    template <typename Message>
    class channel {
        friend class subscriber<Message>;

    public:
        channel () = default;
        ~channel () noexcept;

        // No copying or assignment.
        channel (channel const &) = delete;
        channel (channel &&) = delete;
        channel & operator= (channel const &) = delete;
        channel & operator= (channel &&) = delete;

        /// Broadcasts a message to all subscribers.
        void publish (Message const & message);

        /// Creates a new subscriber instance and attaches it to this channel.
        std::unique_ptr<subscriber<Message>> new_subscriber ();

        /// Cancels a subscription.
        ///
        /// The subscription is marked as inactive. If waiting it is woken up.
        void cancel (subscriber<Message> & sub) const;

    private:
        template <typename WaitFunction, typename = std::enable_if_t<std::is_invocable_r_v<bool,  WaitFunction, std::condition_variable&, std::unique_lock<std::mutex>&>>>
        std::optional<Message> wait (subscriber<Message> &sub, WaitFunction wfn);

        /// Called when a subscriber is destructed to remove it from the subscribers list.
        void remove_sub (subscriber<Message> * sub) noexcept;

        mutable std::mutex mut_;
        mutable std::condition_variable cv_;

        /// All of the subscribers to this channel.
        std::unordered_set<subscriber<Message> *> subscribers_;
    };

    //*          _               _ _              *
    //*  ____  _| |__ ___ __ _ _(_) |__  ___ _ _  *
    //* (_-< || | '_ (_-</ _| '_| | '_ \/ -_) '_| *
    //* /__/\_,_|_.__/__/\__|_| |_|_.__/\___|_|   *
    //*                                           *
    // (dtor)
    // ~~~~~~
    template <typename Message>
    subscriber<Message>::~subscriber () noexcept {
      owner_->remove_sub (this);
    }

    // wait
    // ~~~~
    template <typename Message>
    std::optional<Message> subscriber<Message>::wait () {
      return owner_->wait (*this, [](std::condition_variable &cv, std::unique_lock<std::mutex> &lock) { cv.wait(lock); return true; });
    }

    // wait for
    // ~~~~~~~~
    template <typename Message>
    template <typename Rep, typename Period>
    std::optional<Message> subscriber<Message>::wait_for (std::chrono::duration<Rep, Period> const & rel_time) {
      return this->wait_until (std::chrono::steady_clock::now() + rel_time);
    }

    // wait until
    // ~~~~~~~~~~
    template <typename Message>
    template <typename Clock, typename Duration>
    std::optional<Message> subscriber<Message>::wait_until (std::chrono::time_point<Clock, Duration> const & timeout_time) {
      return owner_->wait (*this, [&timeout_time] (std::condition_variable &cv, std::unique_lock<std::mutex> &lock) { return cv.wait_until(lock, timeout_time) ==  std::cv_status::no_timeout; });
    }

    // cancel
    // ~~~~~~
    template <typename Message>
    void subscriber<Message>::cancel () {
      this->owner ().cancel (*this);
    }


    //*     _                       _  *
    //*  __| |_  __ _ _ _  _ _  ___| | *
    //* / _| ' \/ _` | ' \| ' \/ -_) | *
    //* \__|_||_\__,_|_||_|_||_\___|_| *
    //*                                *
    // (dtor)
    // ~~~~~~
    template <typename Message>
    channel<Message>::~channel () noexcept {
        // Check that this channel has no subscribers.
        assert (subscribers_.empty ());
    }

    // publish
    // ~~~~~~~
    template <typename Message>
    void channel<Message>::publish (Message const & message) {
        std::lock_guard<std::mutex> _{mut_};
        if (subscribers_.size () > 0) {
            for (auto & sub : subscribers_) {
                sub->queue_.push (message);
            }
            cv_.notify_all ();
        }
    }

    // new subscriber
    // ~~~~~~~~~~~~~~
    template <typename Message>
    std::unique_ptr<subscriber<Message>> channel<Message>::new_subscriber () {
        std::lock_guard<std::mutex> lock{mut_};
        auto resl = std::make_unique<subscriber<Message>>(this);
        subscribers_.insert (resl.get ());
        return resl;
    }

    // cancel
    // ~~~~~~
    template <typename Message>
    void channel<Message>::cancel (subscriber<Message> & sub) const {
        if (&sub.owner () == this) {
            std::unique_lock<std::mutex> lock{mut_};
            sub.active_ = false;
            cv_.notify_all ();
        }
    }

    // remove sub
    // ~~~~~~~~~~
    template <typename Message>
    void channel<Message>::remove_sub (subscriber<Message> * sub) noexcept {
        std::lock_guard<std::mutex> _{mut_};
        assert (subscribers_.find (sub) != subscribers_.end ());
        subscribers_.erase (sub);
    }

    // listen
    // ~~~~~~
    template <typename Message>
    template <typename WaitFunction, typename>
    std::optional<Message> channel<Message>::wait (subscriber<Message> & sub, WaitFunction wfn) {
        std::unique_lock<std::mutex> lock{mut_};
        while (sub.active ()) {
            if (sub.queue_.empty ()) {
                if (!wfn (std::ref(cv_), std::ref(lock))) {
                  return {}; // timeout
                }
            } else {
                auto const message = std::move (sub.queue_.front ());
                sub.queue_.pop ();
                return message;
            }
        }
        return {};
    }

} // end namespace pubsub

#endif // PUBSUB_HPP
