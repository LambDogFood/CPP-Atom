//
// Created by Alex Edgar on 13/02/2026.
//

#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <atomic>
#include <string>
#include "atom.h"

// Error handler
auto testErrorHandler = [](const std::exception_ptr& e) {
    try { std::rethrow_exception(e); }
    catch (const std::exception& ex) {
        std::cerr << "Listener error: " << ex.what() << std::endl;
    }
};

// General
void test_initial_value() {
    auto atom = createAtom<int>(42, testErrorHandler);
    assert(atom->get() == 42);
}

void test_set_and_get() {
    auto atom = createAtom<int>(0, testErrorHandler);
    atom->set(5);
    assert(atom->get() == 5);
}

void test_update() {
    auto atom = createAtom<int>(10, testErrorHandler);
    atom->update([](const int& v) { return v + 5; });
    assert(atom->get() == 15);
}

void test_multiple_updates() {
    auto atom = createAtom<int>(0, testErrorHandler);
    for (int i = 0; i < 100; i++) {
        atom->update([](const int& v) { return v + 1; });
    }
    assert(atom->get() == 100);
}

// Subscription
void test_subscribe_fires() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int received = -1;
    auto sub = atom->subscribe([&](const int& v) { received = v; });
    atom->set(42);
    assert(received == 42);
}

void test_subscribe_fires_on_update() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int received = -1;
    auto sub = atom->subscribe([&](const int& v) { received = v; });
    atom->update([](const int& v) { return v + 10; });
    assert(received == 10);
}

void test_multiple_subscribers() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int a = 0, b = 0, c = 0;
    auto sub1 = atom->subscribe([&](const int& v) { a = v; });
    auto sub2 = atom->subscribe([&](const int& v) { b = v; });
    auto sub3 = atom->subscribe([&](const int& v) { c = v; });
    atom->set(7);
    assert(a == 7);
    assert(b == 7);
    assert(c == 7);
}

// Unsubscribe
void test_raii_unsubscribe() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int count = 0;
    {
        auto sub = atom->subscribe([&](const int&) { count++; });
        atom->set(1);
        atom->set(2);
    }
    atom->set(3);
    assert(count == 2);
}

void test_manual_unsubscribe() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int count = 0;
    auto sub = atom->subscribe([&](const int&) { count++; });
    atom->set(1);
    sub.unsubscribe();
    atom->set(2);
    assert(count == 1);
}

void test_double_unsubscribe() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int count = 0;
    auto sub = atom->subscribe([&](const int&) { count++; });
    sub.unsubscribe();
    sub.unsubscribe();  // Should be safe
    atom->set(1);
    assert(count == 0);
}

void test_move_subscription() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int count = 0;
    auto sub1 = atom->subscribe([&](const int&) { count++; });
    auto sub2 = std::move(sub1);
    atom->set(1);
    assert(count == 1);  // Still alive via sub2
}

void test_move_assign_subscription() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int countA = 0, countB = 0;
    auto sub = atom->subscribe([&](const int&) { countA++; });
    atom->set(1);
    assert(countA == 1);

    sub = atom->subscribe([&](const int&) { countB++; });  // Old sub unsubscribes
    atom->set(2);
    assert(countA == 1);  // Old listener didn't fire again
    assert(countB == 1);  // Mew listener fired
}

// Equality skip
void test_skip_equal_set() {
    auto atom = createAtom<int>(5, testErrorHandler);
    int count = 0;
    auto sub = atom->subscribe([&](const int&) { count++; });
    atom->set(5);
    assert(count == 0);
}

void test_skip_equal_update() {
    auto atom = createAtom<int>(5, testErrorHandler);
    int count = 0;
    auto sub = atom->subscribe([&](const int&) { count++; });
    atom->update([](const int& v) { return v; });  // returns same value
    assert(count == 0);
}

// Type issues
void test_string_atom() {
    auto atom = createAtom<std::string>("hello", testErrorHandler);
    std::string received;
    auto sub = atom->subscribe([&](const std::string& v) { received = v; });
    atom->set("world");
    assert(received == "world");
    assert(atom->get() == "world");
}

void test_vector_atom() {
    auto atom = createAtom<std::vector<int>>({1, 2, 3}, testErrorHandler);
    auto sub = atom->subscribe([](const std::vector<int>&) {});
    atom->set({4, 5, 6});
    assert(atom->get().size() == 3);
    assert(atom->get()[0] == 4);
}

// Exceptions
void test_throwing_callback_doesnt_kill_others() {
    auto atom = createAtom<int>(0, testErrorHandler);
    int received = -1;

    auto sub1 = atom->subscribe([](const int&) {
        throw std::runtime_error("boom");
    });
    auto sub2 = atom->subscribe([&](const int& v) {
        received = v;
    });

    atom->set(10);
    assert(received == 10);
}

void test_error_handler_receives_exception() {
    std::string errorMsg;
    auto atom = createAtom<int>(0, [&](std::exception_ptr e) {
        try { std::rethrow_exception(e); }
        catch (const std::exception& ex) { errorMsg = ex.what(); }
    });

    auto sub = atom->subscribe([](const int&) {
        throw std::runtime_error("test error");
    });

    atom->set(1);
    assert(errorMsg == "test error");
}

// Lifetime
void test_subscription_outlives_atom() {
    Subscription<int> sub = Subscription<int>(std::weak_ptr<Atom<int>>{}, 0);
    {
        auto atom = createAtom<int>(0, testErrorHandler);
        sub = atom->subscribe([](const int&) {});
    }
    // Atom is dead, sub destructor should not crash
}

// Concurrency
void test_concurrent_writes() {
    auto atom = createAtom<int>(0, testErrorHandler);
    std::atomic<int> notifications{0};
    auto sub = atom->subscribe([&](const int&) { notifications++; });

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < 1000; j++) {
                atom->set(i * 1000 + j);
            }
        });
    }
    for (auto& t : threads) t.join();
    // No crashes, no deadlocks
}

void test_concurrent_subscribe_unsubscribe() {
    auto atom = createAtom<int>(0, testErrorHandler);
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; j++) {
                auto sub = atom->subscribe([](const int&) {});
                atom->set(j);
            }
        });
    }
    for (auto& t : threads) t.join();
}

void test_concurrent_reads_and_writes() {
    auto atom = createAtom<int>(0, testErrorHandler);
    std::atomic<bool> done{false};

    std::vector<std::thread> readers;
    for (int i = 0; i < 5; i++) {
        readers.emplace_back([&]() {
            while (!done) {
                volatile int v = atom->get();
                (void)v;
            }
        });
    }

    std::vector<std::thread> writers;
    for (int i = 0; i < 5; i++) {
        writers.emplace_back([&, i]() {
            for (int j = 0; j < 1000; j++) {
                atom->set(i * 1000 + j);
            }
        });
    }

    for (auto& t : writers) t.join();
    done = true;
    for (auto& t : readers) t.join();
}

// Test runner
void run(const char* name, void(*fn)()) {
    try {
        fn();
        std::cout << "  PASS  " << name << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  FAIL  " << name << " — " << e.what() << std::endl;
    } catch (...) {
        std::cout << "  FAIL  " << name << " — unknown exception" << std::endl;
    }
}

int main() {
    std::cout << "\n=== Atom Tests ===" << std::endl;

    std::cout << "\n--- General ---" << std::endl;
    run("initial value", test_initial_value);
    run("set and get", test_set_and_get);
    run("update", test_update);
    run("multiple updates", test_multiple_updates);

    std::cout << "\n--- Subscription ---" << std::endl;
    run("subscribe fires", test_subscribe_fires);
    run("subscribe fires on update", test_subscribe_fires_on_update);
    run("multiple subscribers", test_multiple_subscribers);

    std::cout << "\n--- Unsubscribe ---" << std::endl;
    run("raii unsubscribe", test_raii_unsubscribe);
    run("manual unsubscribe", test_manual_unsubscribe);
    run("double unsubscribe", test_double_unsubscribe);
    run("move subscription", test_move_subscription);
    run("move assign subscription", test_move_assign_subscription);

    std::cout << "\n--- Equality skip ---" << std::endl;
    run("skip equal set", test_skip_equal_set);
    run("skip equal update", test_skip_equal_update);

    std::cout << "\n--- Type issues ---" << std::endl;
    run("string atom", test_string_atom);
    run("vector atom", test_vector_atom);

    std::cout << "\n--- Exceptions ---" << std::endl;
    run("throwing callback", test_throwing_callback_doesnt_kill_others);
    run("error handler receives", test_error_handler_receives_exception);

    std::cout << "\n--- Lifetime ---" << std::endl;
    run("subscription outlives atom", test_subscription_outlives_atom);

    std::cout << "\n--- Concurrency ---" << std::endl;
    run("concurrent writes", test_concurrent_writes);
    run("concurrent subscribe/unsubscribe", test_concurrent_subscribe_unsubscribe);
    run("concurrent reads and writes", test_concurrent_reads_and_writes);

    std::cout << "\n=== Done ===" << std::endl;
    return 0;
}
