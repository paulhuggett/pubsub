#ifndef PUBSUB_HPP
#define PUBSUB_HPP

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>

#include <future>

namespace pubsub {

    class channel;

    //*          _               _ _              *
    //*  ____  _| |__ ___ __ _ _(_) |__  ___ _ _  *
    //* (_-< || | '_ (_-</ _| '_| | '_ \/ -_) '_| *
    //* /__/\_,_|_.__/__/\__|_| |_|_.__/\___|_|   *
    //*                                           *
    class subscriber {
        friend class channel;
    public:
        ~subscriber () noexcept;

        // No copying or assignment.
        subscriber (subscriber const & ) = delete;
        subscriber (subscriber && ) = delete;
        subscriber & operator= (subscriber const & ) = delete;
        subscriber & operator= (subscriber && ) = delete;

        /// \tparam Function A function with a signature compatible with: std::function<void(std::string const &)>.
        /// \param f  A function which will be called when a message is published on the owning channel.
        /// \param args  Arguments that will be passed to f.
        template <typename Function, typename ...Args>
        void listen_sync (Function f, Args && ...args);

        /// \tparam Function A function with a signature compatible with: std::function<void(std::string const &)>.
        /// \param f  A function which will be called when a message is published on the owning channel.
        /// \param args  Arguments that will be passed to f.
        template <typename Function, typename ...Args>
        std::thread listen (Function f, Args && ...args);

    private:
        explicit subscriber (channel * c) : owner_ {c} {}

        std::queue<std::string> queue_;
        channel * const owner_;
        bool active_ = true;
    };

    //*     _                       _  *
    //*  __| |_  __ _ _ _  _ _  ___| | *
    //* / _| ' \/ _` | ' \| ' \/ -_) | *
    //* \__|_||_\__,_|_||_|_||_\___|_| *
    //*                                *
    class channel {
        friend class subscriber;
    public:
        channel () = default;
        channel (channel const & ) = delete;
        channel (channel && ) = delete;
        channel & operator= (channel const & ) = delete;
        channel & operator= (channel && ) = delete;

        /// Broadcasts a message to all subscribers.
        void publish (std::string const & message);

        std::unique_ptr<subscriber> new_subscriber ();
        void unsubscribe (subscriber & sub) const;

    private:
        /// \tparam Function A function with a signature compatible with: std::function<void(std::string const &)>.
        /// \param f  A function which will be called when a message is published on the owning channel.
        template <typename Function, typename ...Args>
        void listen (subscriber * const sub, Function f, Args && ...args);

        void remove_sub (subscriber * sub) noexcept;

        mutable std::mutex mut_;
        mutable std::condition_variable cv_;
        std::unordered_set<subscriber *> subscribers_;
    };

    // subscribe
    // ~~~~~~~~~
    template <typename Function, typename ...Args>
    void channel::listen (subscriber * const sub, Function f, Args && ...args) {
        std::unique_lock<std::mutex> lock {mut_};
        while (sub->active_) {
            cv_.wait (lock);

            while (sub->active_ && sub->queue_.size () > 0) {
                std::string const message = std::move (sub->queue_.front ());
                sub->queue_.pop ();

                // Don't hold the lock whilst the user callback is called: we can't assume
                // that it will return quickly.
                lock.unlock ();
                f (message, std::forward<Args> (args)...);
                lock.lock ();
            }
        }
    }

    //*          _               _ _              *
    //*  ____  _| |__ ___ __ _ _(_) |__  ___ _ _  *
    //* (_-< || | '_ (_-</ _| '_| | '_ \/ -_) '_| *
    //* /__/\_,_|_.__/__/\__|_| |_|_.__/\___|_|   *
    //*                                           *
    // listen_sync
    // ~~~~~~~~~~~
    template <typename Function, typename ...Args>
    void subscriber::listen_sync (Function f, Args && ...args) {
        owner_->listen (this, f, std::forward<Args> (args)...);
    }

    // listen
    // ~~~~~~
    template <typename Function, typename ...Args>
    std::thread subscriber::listen (Function f, Args && ...args) {
        return std::thread {[this] (Function f2, Args ...args2) {
            this->listen_sync (f2, std::forward<Args> (args2)...);
        }, f, std::forward<Args> (args)...};
    }

} // end namespace pubsub

#endif // PUBSUB_HPP
