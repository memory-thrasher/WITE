namespace WITE::Util {

  template<size_t start, size_t deleteCount, class... Tup> struct tuple_splice;

  template<size_t start, size_t deleteCount, class Head, class... Tail>
  struct tuple_splice<start, deleteCount, std::tuple<Head, Tail...>> {
    using rest = tuple_splice<start-1, deleteCount, std::tuple<Tail...>>;
    using Right = typename rest::Right;
    using Left = std::tuple<Head, typename rest::Left>;
    using Outer = std::tuple<Left, Right>;
  };

  template<size_t deleteCount, class Head, class... Tail>
  struct tuple_splice<1, deleteCount, std::tuple<Head, Tail...>> :
    tuple_splice<0, deleteCount, std::tuple<Tail...>> {
    using Left = Head;
  };

  template<size_t deleteCount, class Head, class... Tail>
  struct tuple_splice<0, deleteCount, std::tuple<Head, Tail...>> :
    tuple_splice<0, deleteCount-1, std::tuple<Tail...>> {
  };

  template<class... Tup> struct tuple_splice<0, 0, Tup...> {
    using Right = std::tuple<Tup...>;
  };

}
