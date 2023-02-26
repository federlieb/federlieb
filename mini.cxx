#include <algorithm>
#include <boost/callable_traits/args.hpp>
#include <boost/callable_traits/class_of.hpp>
#include <boost/mp11.hpp>
#include <boost/stl_interfaces/iterator_interface.hpp>
#include <boost/dll.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <numeric>
#include <random>
#include <ranges>
#include <source_location>
#include <sqlite3.h>
#include <sys/socket.h>
#include <vector>

#include <boost/graph/graph_concepts.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/json.hpp>

#include <boost/pfr.hpp>

#include <linux/if_alg.h>
#include <linux/socket.h>
#include <sys/socket.h>


void unparenthesize(std::string& s) {
  s.erase(0, s.find_first_of("(") + 1);
  s.erase(s.find_last_of(")"));
}


int
main()
{

  struct transition {
    uint32_t src;
    uint32_t via;
    uint32_t dst;
  };

  std::vector<transition> transitions;

  transitions.push_back({1,2,3});
  transitions.push_back({1,1,4});
  transitions.push_back({3,1,3});
  transitions.push_back({4,2,1});
  transitions.push_back({2,2,4});

  std::ranges::sort(transitions, {}, [](auto&& e){
    return std::make_tuple(e.src, e.via, e.dst);
  });

  std::vector<std::pair<size_t, size_t>> index;

  auto max_src = std::ranges::max(transitions, {}, [](auto&& e) { return e.src; });
  index.resize(max_src.src);

  auto prev = transitions.front().src;

  index[prev].first = 0;

  for (size_t ix = 0; ix < transitions.size(); ++ix) {
    auto&& e = transitions[ix];
    if (e.src != prev) {
      index[prev].second = ix;
      index[e.src].first = ix;
      prev = e.src;
    }
  }

  index[prev].second = transitions.size();

  std::vector<transition> result;

  std::copy(transitions.begin() + index[2].first, transitions.begin() + index[2].second, std::back_inserter(result));
  std::copy(transitions.begin() + index[1].first, transitions.begin() + index[1].second, std::back_inserter(result));

  std::ranges::sort(result, {}, [](auto&& e) {
    return std::make_tuple(e.via, e.dst);
  });

  std::ranges::unique(result, {}, [](auto&& e) {
    return std::make_tuple(e.via, e.dst);
  });

  return 0;
}

#if 0
int
main()
{

  // auto p = boost::filesystem::path();
  // auto x = boost::dll::import_symbol(p, "sqlite3_open");

  auto pl = boost::dll::program_location();
  auto k = pl.c_str();
  auto x = boost::dll::shared_library(pl);
  auto y = x.has("sqlit3_open_v2");
  auto z = x.has("_sqlit3_open");
  // auto m = boost::dll::import_symbol<int(char*,sqlite3*)>(pl, "sqlite3_open", boost::dll::load_mode::rtld_lazy);
  auto u = dlsym(RTLD_NEXT, "sqlite3_open");

  sqlite3* db;
  int rc = sqlite3_open(":memory:", &db);

  return 0;
}
#endif

#if 0
struct convA
{

  template<typename T>
  operator std::optional<T>()
  {
    return T{};
  }

  template<typename T>
  operator T()
  {
    return T{};
  }
};

// template convA::operator std::optional<int>();

void
foo(std::optional<int>)
{}

void
bar(int)
{}

int
main()
{

  foo(convA{});
  bar(convA{});

  return 0;
}

#if 0

int
main()
{

  constexpr auto SHA256_DIG_LEN = 32;
  unsigned char digest[SHA256_DIG_LEN];

  struct sockaddr_alg sa_alg;
  memset(&sa_alg, 0, sizeof(sa_alg));

  sa_alg.salg_family = AF_ALG;
  memcpy(sa_alg.salg_type, "hash", 5);
  memcpy(sa_alg.salg_name, "sha256", 7);

  int sock_fd;
  int err;

  sock_fd = socket(AF_ALG, SOCK_SEQPACKET, 0);

  if (sock_fd < 0) {
    perror("failed to allocate socket\n");
    return -1;
  }

  err = bind(sock_fd, (struct sockaddr*)&sa_alg, sizeof(sa_alg));

  if (err) {
    perror("failed to bind socket, alg may not be supported\n");
    return -EAFNOSUPPORT;
  }

  int fd = accept(sock_fd, NULL, 0);
  if (fd < 0) {
    perror("failed to open connection for the socket\n");
    return -EBADF;
  }

  auto plaintext = "Hello";

  int text_len = strlen(plaintext);
  err = write(fd, plaintext, text_len);
  if (err != text_len) {
    perror("something went wrong while writing data to fd\n");
    return -1;
  }
  read(fd, digest, SHA256_DIG_LEN);

  close(fd);
  close(sock_fd);

  /* Print digest to output */
  for (int i = 0; i < SHA256_DIG_LEN; i++) {
    printf("%02x", digest[i]);
  }
  printf("\n");

  return 0;
}

#if 0

void
foo(int, int, int)
{}

void
foo(int, int, int, int, int)
{}

struct X
{
  void xFunc() {}
  void xFunc(int&) {}
  void xFunc(int, int, int = 7) {}
  void xFunc(int, int, int, int, int) {}
};

struct pseudo_converter
{
  size_t ignored;

  template<typename Wanted>
  operator Wanted&()
  {
    return std::declval<Wanted>();
  }
};

template<typename X>
struct scalar_callable_with_n
{
  template<size_t... Arg>
  static constexpr bool check(std::index_sequence<Arg...>)
  {
    return requires { std::declval<X>().xFunc(pseudo_converter{ Arg }...); };
  }

  template<typename N>
  using fn = std::bool_constant<check(std::make_index_sequence<N::value>())>;
};

int
main()
{

  using q = boost::mp11::mp_copy_if_q<boost::mp11::mp_iota_c<10>,
                                      scalar_callable_with_n<X>>;

  boost::mp11::mp_for_each<q>(
    [](auto&& e) { std::clog << e.value << std::endl; });

  return 0;
}

#if 0

namespace outer {
namespace inner {
struct S
{
  void m();
};
}
}

namespace fl = outer;

void
fl::inner::S::m()
{}

namespace federlieb {
namespace inner {
struct n
{
  void m();
};
}
}

void
federlieb::inner::n::m()
{}

struct bar
{
  int c;
};

struct foo
{
  int a;
  float b;
  bar child;
};

void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const bar& ours)
{
  // theirs = { { "a", ours.c } };
  theirs = nullptr;
}

void
tag_invoke(const boost::json::value_from_tag&,
           boost::json::value& theirs,
           const foo& ours)
{
  theirs = { { "c", ours.a }, { "b", ours.b }, { "child", ours.child } };
}

#if 0
auto
array_lt(const boost::json::array& lhs, const boost::json::array& rhs)
{
  return std::ranges::lexicographical_compare(lhs, rhs);
}
#endif

struct formatter
{
  template<std::integral T>
  std::string operator()(const T& value)
  {
    return std::to_string(value);
  }

  std::string operator()(const std::string& value) { return value; }
};

template<typename... Ts>
std::string
format(const std::string& format, Ts... args)
{
  if (format.empty()) {
    return "";
  }

  formatter f;
  std::vector<std::string> values{ (f(args), ...) };
  std::stringstream s;

  size_t ix = 0;

  bool open = false;

  for (auto it = format.begin(); it != format.end(); ++it) {
    if (!open && *it == '{') {
      open = true;
    } else if (!open) {
      s << *it;
    } else if (*it == '}') {
      s << values.at(ix++);
      open = false;
    } else {
      std::terminate();
    }
  }

  return s.str();
}

int
main()
{

  std::cout << format("hello{}sss", 237) << std::endl;
  std::cout << format("", 237) << std::endl;

  return 0;
}

#if 0

class Y
{
public:
  Y(int) {}
};

class X
{
public:
  int xFunc() { return 0; }
  int xFunc(int, int = 7) { return 2; }
  int xFunc(double, std::shared_ptr<int*>, std::vector<bool>, Y) { return 0; }
};

struct prober
{
  size_t ignored;
  template<typename T>
  operator T()
  {
    return std::declval<T>();
  }
};

template<size_t... N>
constexpr bool
probe(std::index_sequence<N...>)
{
  return requires { std::declval<X>().xFunc(prober{ N }...); };
}

template<size_t... N>
struct foo
{
  static constexpr bool value = requires
  {
    std::declval<X>().xFunc(prober{ N }...);
  };
  using type = int;
};

template<class... P>
struct bar
{
  using type = int;
  static auto const value = true;
};

template<class... T>
struct buz
{
  // using type = int;
  static auto const value = true;
};

int
main()
{

  auto t0 = probe(std::make_index_sequence<0>());
  auto t1 = probe(std::make_index_sequence<1>());
  auto t2 = probe(std::make_index_sequence<2>());
  auto t3 = probe(std::make_index_sequence<3>());
  auto t4 = probe(std::make_index_sequence<4>());
  auto t5 = probe<0, 1, 2, 3, 4, 5>({});

  std::clog << foo<>::value << std::endl;
  std::clog << foo<0>::value << std::endl;
  std::clog << foo<0, 1>::value << std::endl;
  std::clog << foo<0, 1, 2>::value << std::endl;
  std::clog << foo<0, 1, 2, 3>::value << std::endl;
  std::clog << foo<0, 1, 2, 3, 4>::value << std::endl;

  // using X = boost::mp11::mp_to_bool<bar<std::index_sequence<3>>>;

  //  X y;

  using H =
    boost::mp11::mp_copy_if<std::tuple<std::integer_sequence<size_t, 1>>, buz>;

  H h;
  auto e = std::get<0>(h);

  std::clog << t0 << ' ' << t1 << ' ' << t2 << ' ' << t3 << ' ' << t4 << '\n';

  auto a1 = nlohmann::json::parse("[1,2,2,4]");
  auto a2 = nlohmann::json::parse("[7,5,4,2]");

  auto new_pos = a1.insert(a1.end(), a2.begin(), a2.end());

  std::clog << a1.dump(-1) << '\n';

  return 0;
}

#if 0

namespace federlieb {
namespace fl = ::federlieb;
using my_type = int;

fl::my_type
f();

fl::my_type
f()
{
  return 77;
}

class fx_json_set_agg
{
public:
  static inline auto const name = "json_set_agg";
  static inline auto const deterministic = true;
  static inline auto const direct_only = false;

  fx_json_set_agg() {}

  int xFunc(int, int, int, int) { return 123; }
  int xStep(int, int, int, int) { return 123; }
  int xInverse(int a, int b, int c = 0)
  {

    std::cerr << "a " << a << " b " << b << " c " << c << '\n';
    return 123;
  }
  int xValue() { return 123; }
  int xFinal() { return 123; }
};

}

namespace sql {
template<typename T>
struct box
{
  box(const T& value)
    : value_(value)
  {}
  const T& value_;
};
}

#if 1
template<auto T, typename... Args>
constexpr bool
is_callable_with_n(std::index_sequence_for<Args...> args)
{
  if constexpr (requires { std::invoke<decltype(T), Args...>(T); }) {
    return true;
  }
  return false;
}
#endif

template<typename T>
struct callable_arity
{};

template<typename Return, typename... Args>
struct callable_arity<Return (*)(Args...)>
  : std::integral_constant<size_t, sizeof...(Args)>
{};

template<typename Return, typename Class, typename... Args>
struct callable_arity<Return (Class::*)(Args...)>
  : std::integral_constant<size_t, sizeof...(Args)>
{};

enum class values_callback
{
  xFunc,
  xStep,
  xInverse
};

template<typename Class, values_callback Cb, typename... Ts>
constexpr size_t
minimum_arguments_helper(std::tuple<Ts...> k)
{
  constexpr auto size = sizeof...(Ts);

  using enum values_callback;

  if constexpr (((Cb == xInverse) &&
                 requires { std::declval<Class>().xInverse(Ts()...); }) ||
                ((Cb == xStep) &&
                 requires { std::declval<Class>().xStep(Ts()...); }) ||
                ((Cb == xFunc) &&
                 requires { std::declval<Class>().xFunc(Ts()...); })) {

    using PopBackOrEmpty =
      boost::mp11::mp_take<std::tuple<Ts...>,
                           std::integral_constant<size_t, size ? size - 1 : 0>>;

    return size ? minimum_arguments_helper<Class, Cb>(PopBackOrEmpty{}) : 0;
  }

  return size + 1;
}

template<auto fn, values_callback id>
constexpr size_t
minimum_arguments()
{
  using Class = boost::callable_traits::class_of_t<decltype(fn)>;
  using Args = boost::callable_traits::args_t<decltype(fn)>;
  using AfterThis = boost::mp11::mp_pop_front<Args>;
  return minimum_arguments_helper<Class, id>(AfterThis{});
}

template<auto fn, values_callback id>
constexpr size_t
maximum_arguments()
{
  using Args = boost::callable_traits::args_t<decltype(fn)>;
  using AfterThis = boost::mp11::mp_pop_front<Args>;
  return std::tuple_size<AfterThis>::value;
}

template<auto fn, values_callback id, size_t... N>
void
reg_n(std::index_sequence<N...>)
{}

template<auto fn, values_callback id, size_t min, size_t... N>
void
reg(std::index_sequence<N...>)
{
  size_t list[] = { (min + N)... };
  for (auto&& e : list) {
    std::cerr << "e " << e << std::endl;
  }

  (reg_n<fn, id>(std::make_index_sequence<min + N + N>()), ...);
}

#if 1
template<auto fn, size_t... N>
void
fun(std::index_sequence<N...>)
{
  using Args = boost::callable_traits::args_t<decltype(fn)>;
  using AfterThis = boost::mp11::mp_pop_front<Args>;

  AfterThis args{};

  (std::get<N>(args), ...);
}
#endif

template<auto fn>
auto
foo()
{
  using Args = boost::callable_traits::args_t<decltype(fn)>;
  using AfterThis = boost::mp11::mp_pop_front<Args>;
  return AfterThis{};
}

int
main()
{

  // int (federlieb::fx_json_set_agg::*xInxverse)(int, int, int, int) = nullptr;
  //    &federlieb::fx_json_set_agg::xInverse;

  constexpr auto min2 = minimum_arguments<&federlieb::fx_json_set_agg::xFunc,
                                          values_callback::xFunc>();
  //  constexpr auto min1 =
  //  minimum_arguments<&federlieb::fx_json_set_agg::xInverse,
  //                                          values_callback::xInverse>();
  auto min1 = 0;

  //  auto r =
  //    fun<&federlieb::fx_json_set_agg::xFunc>(std::make_index_sequence<min2>());

  auto mmm = foo<&federlieb::fx_json_set_agg::xInverse>();
  auto& sdlkfjd = std::get<2>(mmm);

  std::cerr << "min1 " << min1 << " min2 " << min2 << std::endl;

#if 0
  constexpr auto max = maximum_arguments<&federlieb::fx_json_set_agg::xInverse,
                                         values_callback::xInverse>();

  reg<&federlieb::fx_json_set_agg::xInverse, values_callback::xInverse, min>(
    std::make_index_sequence<max - min + 1>());

  std::cerr << minimum_arguments<&federlieb::fx_json_set_agg::xInverse,
                                 values_callback::xInverse>()
            << std::endl;

  std::tuple<int, int, int> p;

  std::cerr << typeid(decltype(&federlieb::fx_json_set_agg::xInverse)).name()
            << std::endl;
#endif
#if 0
  using G = boost::callable_traits::args_t<
    decltype(&federlieb::fx_json_set_agg::xInverse)>;

  for (size_t ix = 0; ix < 1; ++ix) {
    using L = boost::mp11::mp_take<boost::mp11::mp_pop_front<G>,
                                   std::integral_constant<size_t, 2>>;
    // L::mek a;
    if constexpr (invoke_xInverse(L{})) {
      std::cerr << "true" << std::endl;
    } else {
      std::cerr << "false" << std::endl;
    }
  }
#endif
  // using D = L::mek;

#if 0
  if constexpr (requires {
                  invoke_xInverse<int, int, int, int>(
                    std::make_index_sequence<4>());
                }) {
    std::cerr << "ok" << '\n';
  }
#endif

#if 0
  auto xm = std::tuple_size<boost::callable_traits::args_t<
    decltype(&federlieb::fx_json_set_agg::xInverse)>>::value;

  if constexpr (requires {
                  std::declval<federlieb::fx_json_set_agg>().xInverse({}, {});
                }) {
    std::cerr << "could call with only `this` + 2 (expected " << xm << " total)"
              << std::endl;
  }
#endif

  //  double f = (false ? {} : {});

  //  federlieb::callable_arity<decltype(&federlieb::fx_json_set_agg::xInverse)>
  //    mmm;
  // std::cerr << xm << '\n';

  //  federlieb::fx_json_set_agg::register_function(1);
  return 0;

  return 0;
}
#endif

#endif

#endif

#endif

#endif
#endif
