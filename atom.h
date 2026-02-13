//
// Created by Alex Edgar on 13/02/2026.
//

#pragma once

#include <shared_mutex>
#include <memory>
#include <functional>
#include <unordered_map>
#include <concepts>
#include <cstdint>
#include <type_traits>

template <typename T>
class Atom;

template <typename T>
class Subscription {
public:
    Subscription(std::weak_ptr<Atom<T>> owner, uint64_t id) : owner_(std::move(owner)), id_(id) {}
    ~Subscription() {
        if (auto atom = owner_.lock()) {
            std::unique_lock lock(atom->mutex_);
            atom->listeners_.erase(id_);
        }
    }

    Subscription(Subscription&& other) noexcept : owner_(std::move(other.owner_)), id_(other.id_) {
        other.id_ = 0;
    }

    void unsubscribe() {
        if (auto atom = owner_.lock()) {
            std::unique_lock lock(atom->mutex_);
            atom->listeners_.erase(id_);
        }
        owner_.reset();
    }

    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            // Unsubscribe from current
            if (auto atom = owner_.lock()) {
                std::unique_lock lock(atom->mutex_);
                atom->listeners_.erase(id_);
            }

            // Steal from other
            owner_ = std::move(other.owner_);
            id_ = other.id_;
            other.id_ = 0;
        }

        return *this;
    }

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;
private:
    std::weak_ptr<Atom<T>> owner_;
    uint64_t id_;
};

template <typename T>
class Atom: public std::enable_shared_from_this<Atom<T>> {
    static_assert(std::is_copy_constructible_v<T>, "T must be copy constructible");
    using ListenerMap = std::unordered_map<uint64_t, std::function<void(const T&)>>;

public:
    struct PrivateKey {
    private:
        PrivateKey() = default;
        template <typename U>
        friend std::shared_ptr<Atom<U>> createAtom(U, std::function<void(std::exception_ptr)> onError);
    };

    explicit Atom(PrivateKey, T initial, std::function<void(std::exception_ptr)> onError) : value_(std::move(initial)), on_error_(std::move(onError)) {}

    T get() const {
        std::shared_lock lock(mutex_);
        return value_;
    }

    void set(T value) {
        ListenerMap snapshot;
        T snapshotValue;
        {
            std::unique_lock lock(mutex_);
            if constexpr (std::equality_comparable<T>) {
                if (value == value_) return;
            }

            if constexpr (std::is_move_assignable_v<T>) {
                value_ = std::move(value);
            } else {
                value_ = value;
            }

            snapshot = listeners_;
            snapshotValue = value_;
        }
        notify(snapshot, snapshotValue);
    }

    void update(std::function<T(const T&)> updater) {
        ListenerMap snapshot;
        T snapshotValue;
        {
            std::unique_lock lock(mutex_);
            auto newValue = updater(value_);
            if constexpr (std::equality_comparable<T>) {
                if (newValue == value_) return;
            }

            if constexpr (std::is_move_assignable_v<T>) {
                value_ = std::move(newValue);
            } else {
                value_ = newValue;
            }

            snapshot = listeners_;
            snapshotValue = value_;
        }
        notify(snapshot, snapshotValue);
    }

    Subscription<T> subscribe(std::function<void(const T&)> callback) {
        std::unique_lock lock(mutex_);
        auto id = next_id_++;
        listeners_[id] = std::move(callback);
        return Subscription<T>(this->shared_from_this(), id);
    }

    Atom(const Atom&) = delete;
    Atom& operator=(const Atom&) = delete;
    Atom(Atom&&) = delete;
    Atom& operator=(Atom&&) = delete;

private:
    friend class Subscription<T>;

    void notify(const ListenerMap& snapshot, const T& value) {
        for (const auto& [id, cb] : snapshot) {
            try {
                cb(value);
            } catch (...) {
                if (on_error_) {
                    on_error_(std::current_exception());
                }
            }
        }
    }

    mutable std::shared_mutex mutex_;
    T value_;
    ListenerMap listeners_;
    uint64_t next_id_{0};
    std::function<void(std::exception_ptr)> on_error_;
};

template <typename T>
std::shared_ptr<Atom<T>> createAtom(T initial, std::function<void(std::exception_ptr)> onError) {
    return std::make_shared<Atom<T>>(typename Atom<T>::PrivateKey{}, std::move(initial), std::move(onError));
}