#ifndef PUBSUB_HPP
#define PUBSUB_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>

namespace pubsub {

    class channel;

    //*          _               _ _              *
    //*  ____  _| |__ ___ __ _ _(_) |__  ___ _ _  *
    //* (_-< || | '_ (_-</ _| '_| | '_ \/ -_) '_| *
    //* /__/\_,_|_.__/__/\__|_| |_|_.__/\___|_|   *
    //*                                           *
    /// An instance of the subscriber class represents a subscription to messages published on an
    /// associated owning channel.
    class subscriber {
        friend class channel;

    public:
        ~subscriber () noexcept;

        // No copying or assignment.
        subscriber (subscriber const &) = delete;
        subscriber (subscriber &&) = delete;
        subscriber & operator= (subscriber const &) = delete;
        subscriber & operator= (subscriber &&) = delete;

        // Blocks waiting for a message to be published on the owning channel of for the
        // subscription to be cancelled.
        std::optional<std::string> listen ();

        /// \returns A reference to the owning channel.
        channel & owner () noexcept { return *owner_; }
        /// \returns A reference to the owning channel.
        channel const & owner () const noexcept { return *owner_; }

    private:
        explicit subscriber (channel * const c) noexcept
                : owner_{c} {
            assert (c != nullptr);
        }

        /// The queue of published messages waiting to be delivered to a listening subscriber.
        ///
        /// \note This is a queue of strings. If there are multiple subscribers to this channel then
        /// the strings will be duplicated in each which could be very inefficient. An alternative
        /// would be to store shared_ptr<string>. For the moment I'm leaving it like this on the
        /// assumption that there will typically be just a single subscriber.
        std::queue<std::string> queue_;

        /// The channel with which this subscription is associated.
        channel * const owner_;

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
    class channel {
        friend class subscriber;

    public:
        channel () = default;

        // No copying or assignment.
        channel (channel const &) = delete;
        channel (channel &&) = delete;
        channel & operator= (channel const &) = delete;
        channel & operator= (channel &&) = delete;

        /// Broadcasts a message to all subscribers.
        void publish (std::string const & message);

        /// Creates a new subscriber instance and attaches it to this channel.
        std::unique_ptr<subscriber> new_subscriber ();

        /// The
        void cancel (subscriber & sub) const;

    private:
        std::optional<std::string> listen (subscriber * const sub);

        /// Called when a subscriber is destructed to remove it from the subscribers list.
        void remove_sub (subscriber * sub) noexcept;

        mutable std::mutex mut_;
        mutable std::condition_variable cv_;

        /// All of the subscribers to this channel.
        std::unordered_set<subscriber *> subscribers_;
    };

} // end namespace pubsub

#endif // PUBSUB_HPP
