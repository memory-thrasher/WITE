namespace WITE::Collections {

  class WriteStream {
  public:
    virtual size_t write(void* src, size_t maxlen) = 0;
    virtual void close() = {};
  };

  class ReadStream {
  public:
    virtual bool empty() = 0;
    virtual size_t read(void* dst, size_t maxlen) = 0;
  }

    stream(WriteStream& out, ReadStream& in) {//like dd, segments as needed
    uint8_t buffer[4096];
    size_t opSize;
    while(!in.empty()) {
      opSize = in.read((void*)buffer, 4096);
      while(opSize)
	opSize -= out.write((void*)buffer, opSize);
    }
  }

}
