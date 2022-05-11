namespace WITE::Collections {

  /*
   * (Runtime) fixed size LIFO queue. Optimized for speed, not thread-safe.
   * (If you want dynamically-sized, see std::stack)
   */
  template<class T> class Stack {
  private:
    std::unique_ptr<T[]> data;
    size_t dataSize, len;
  public:
    Stack(size_t size) : dataSize(size), data(std::make_unique<T[]>(size)) {};
    T& pop() { return data[--len]; };
    void push(T t) { data[len++] = t; };
    size_t size() { return len; };
    size_t capacity() { return dataSize; };
    size_t freeSpace() { return capacity() - size(); };
  }

}
