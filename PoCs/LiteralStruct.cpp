class Fred {
public:
class Foo {
public:
  const bool b;
  constexpr ~Foo() = default;
  constexpr Foo(const bool b) : b(b) {};
};

class Bar {
public:
  static constexpr Foo tru { true };
};

};

int main(int argc, char** argv) {
  return Fred::Bar::tru.b ? 0 : 1;
}

