#include <iostream>
#include "atom.h"

int main() {

    auto count = createAtom<int>(0, [](const std::exception_ptr& e) {
        try { std::rethrow_exception(e); }
        catch (const std::exception& ex) {
            std::cerr << "Listener error: " << ex.what() << std::endl;
        }
    });

    auto sub = count->subscribe([](const auto& value) {
       std::cout << "count changed: " << value << std::endl;
    });

    int current = count->get();
    std::cout << "current changed: " << current << std::endl;

    count->set(5);

    count->update([](const int& prev) {
       return prev + 10;
    });

    {
        auto sub2 = count->subscribe([](const auto& value) {
            std::cout << "count changed: " << value << std::endl;
        });
        count->set(3);
    }

    count->set(10);

    sub.unsubscribe();

    count->set(1);

    return 0;
}