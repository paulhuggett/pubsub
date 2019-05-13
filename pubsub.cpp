#include "pubsub.hpp"

#include <cassert>

namespace pubsub {

    //*          _               _ _              *
    //*  ____  _| |__ ___ __ _ _(_) |__  ___ _ _  *
    //* (_-< || | '_ (_-</ _| '_| | '_ \/ -_) '_| *
    //* /__/\_,_|_.__/__/\__|_| |_|_.__/\___|_|   *
    //*                                           *
    subscriber::~subscriber () noexcept { owner_->remove_sub (this); }


    //*     _                       _  *
    //*  __| |_  __ _ _ _  _ _  ___| | *
    //* / _| ' \/ _` | ' \| ' \/ -_) | *
    //* \__|_||_\__,_|_||_|_||_\___|_| *
    //*                                *
    // publish
    // ~~~~~~~
    void channel::publish (std::string const & message) {
        std::lock_guard<std::mutex> _{mut_};
        if (subscribers_.size () > 0) {
            for (auto & sub : subscribers_) {
                sub->queue_.push (message);
            }
            cv_.notify_all ();
        }
    }

    // new_subscriber
    // ~~~~~~~~~~~~~~
    std::unique_ptr<subscriber> channel::new_subscriber () {
        std::lock_guard<std::mutex> lock{mut_};
        auto resl = std::unique_ptr<subscriber>{new subscriber (this)};
        subscribers_.insert (resl.get ());
        return resl;
    }

    // unsubscribe
    // ~~~~~~~~~~~
    void channel::unsubscribe (subscriber & sub) const {
        std::unique_lock<std::mutex> lock{mut_};
        sub.active_ = false;
        cv_.notify_all ();
    }

    // remove_sub
    // ~~~~~~~~~~
    void channel::remove_sub (subscriber * sub) noexcept {
        std::lock_guard<std::mutex> _{mut_};
        assert (subscribers_.find (sub) != subscribers_.end ());
        subscribers_.erase (sub);
    }

} // end namespace pubsub
