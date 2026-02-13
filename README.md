# CPP-Atom

A thread-safe, reactive state primitive for C++20.

## Features
- Thread-safe
- RAII Subscription lifetime management
- Equality-based skipping
- Exception-safe listener notifications

## Usage
```cpp
auto count = createAtom(0, [](const std::exception_ptr e) {
  try { std::rethrow_exception(e); }
  catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
  }
}

auto sub = count->subscribe([](const int& value) {
  std::cout << "changed: " << value << std::endl;
}

count->get(); // Read
count->set(5); // Write
count->update([](const int& prev) { return prev + 1; }); // Read-Modify-Write
sub.unsubscribe(); // Manual cleanup (or let RAII handle it)
```

## License
MIT
