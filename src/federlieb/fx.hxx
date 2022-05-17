#ifndef FEDERLIEB_FX_HXX
#define FEDERLIEB_FX_HXX

#include <boost/mp11.hpp>

#include "federlieb/api.hxx"
#include "federlieb/context.hxx"
#include "federlieb/db.hxx"
#include "federlieb/detail.hxx"
#include "federlieb/error.hxx"
#include "federlieb/value.hxx"

namespace federlieb::fx {

namespace fl = ::federlieb;

enum class callback_selector
{
  xFunc,
  xStep,
  xInverse,
  xFinal,
  xValue
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

struct coercive_converter
{
  sqlite3_value* value;
  fl::value::coercion coercion{};

  template<typename T>
  operator T()
  {
    T result;
    coercion(value, result);
    return result;
  }

  operator std::optional<std::string>() { return optional<std::string>(); }

  operator std::optional<fl::blob_type>() { return optional<fl::blob_type>(); }

  //  operator std::optional<fl::value::blob>()
  //  {
  //    return optional<fl::value::blob>();
  //  }
  //  operator std::optional<fl::value::text>()
  //  {
  //    return optional<fl::value::text>();
  //  }

  template<typename T>
  std::optional<T> optional()
  {
    std::optional<T> result;
    coercion(value, result);
    return result;
  }
};

template<typename FxClass, callback_selector cb>
struct callable_with_n
{
  template<size_t... Arg>
  static constexpr bool check(std::index_sequence<Arg...>)
  {
    switch (cb) {
      case callback_selector::xFunc:
        return requires
        {
          std::declval<FxClass>().xFunc(pseudo_converter{ Arg }...);
        };
        case callback_selector::xStep: return requires
        {
          std::declval<FxClass>().xStep(pseudo_converter{ Arg }...);
        };
        case callback_selector::xInverse: return requires
        {
          std::declval<FxClass>().xInverse(pseudo_converter{ Arg }...);
        };
        default: return false;
    }
  }

  template<typename N>
  using fn = std::bool_constant<check(std::make_index_sequence<N::value>())>;
};

template<typename Derived>
class base
{
private:
  template<callback_selector cb>
  static auto make_simple_lambda()
  {

    return [](sqlite3_context* theirs) noexcept -> void {
      //

      auto ctx = fl::context(theirs);

      auto impl_ptr = static_cast<Derived**>(
        sqlite3_aggregate_context(theirs, sizeof(Derived*)));

      if (nullptr == impl_ptr) {
        ctx.error_nomem();
        return;
      }

      if (nullptr == *impl_ptr) {
        try {
          *impl_ptr = new Derived;
        } catch (...) {
          ctx.error_nomem();
          return;
        }
      }

      try {

        if constexpr (cb == callback_selector::xFinal) {
          auto result = (*impl_ptr)->xFinal();
          ctx.result(result);
          delete *impl_ptr;

        } else if constexpr (cb == callback_selector::xValue) {
          auto result = (*impl_ptr)->xValue();
          ctx.result(result);
        }

      } catch (...) {
        ctx.error("exception");
      }
    };
  }

  template<auto fn, callback_selector cb, std::size_t... N>
  static auto make_values_lambda(std::index_sequence<N...>)
  {

    return [](sqlite3_context* theirs,
              int argc,
              sqlite3_value** argv) noexcept -> void {
      //

      auto ctx = fl::context(theirs);
      auto impl_ptr = static_cast<Derived**>(
        sqlite3_aggregate_context(theirs, sizeof(Derived*)));

      if (nullptr == impl_ptr) {
        ctx.error_nomem();
        return;
      }

      if (nullptr == *impl_ptr) {
        try {
          *impl_ptr = new Derived;
        } catch (...) {
          ctx.error_nomem();
          return;
        }
      }

      try {

        if constexpr (cb == callback_selector::xStep) {
          (*impl_ptr)->xStep(coercive_converter{ *(argv + N) }...);
        }

        if constexpr (cb == callback_selector::xInverse) {
          (*impl_ptr)->xInverse(coercive_converter{ *(argv + N) }...);
        }

      } catch (...) {
        ctx.error("exception");
      }
    };
  }

  template<auto fn, std::size_t... N>
  static auto make_scalar_lambda(std::index_sequence<N...>)
  {

    return [](sqlite3_context* theirs,
              int argc,
              sqlite3_value** argv) noexcept -> void {
      //

      auto ctx = fl::context(theirs);
      auto impl_ptr = static_cast<Derived*>(sqlite3_user_data(theirs));

      try {

        auto result = impl_ptr->xFunc(coercive_converter{ *(argv + N) }...);
        ctx.result(result);

      } catch (...) {
        ctx.error("exception");
      }
    };
  }

public:
  static void register_function(fl::db& db)
  {

    constexpr std::size_t probe_max = 8;

    using sig_xfunc = boost::mp11::mp_copy_if_q<
      boost::mp11::mp_iota_c<probe_max>,
      callable_with_n<Derived, callback_selector::xFunc>>;

    using sig_xstep = boost::mp11::mp_copy_if_q<
      boost::mp11::mp_iota_c<probe_max>,
      callable_with_n<Derived, callback_selector::xStep>>;

    using sig_xinverse = boost::mp11::mp_copy_if_q<
      boost::mp11::mp_iota_c<probe_max>,
      callable_with_n<Derived, callback_selector::xInverse>>;

    // Error when no suitable callback for scalar or aggregate.
    static_assert(!(boost::mp11::mp_empty<sig_xfunc>::value &&
                    boost::mp11::mp_empty<sig_xstep>::value));

    int flags = SQLITE_UTF8;

    if constexpr (requires { Derived::deterministic; }) {
      if (Derived::deterministic) {
        flags |= SQLITE_DETERMINISTIC;
      }
    }

    if constexpr (requires { Derived::direct_only; }) {
      if (Derived::direct_only) {
        flags |= SQLITE_DIRECTONLY;
      }
    }

    if constexpr (!boost::mp11::mp_empty<sig_xfunc>::value) {

      // scalar

      boost::mp11::mp_for_each<sig_xfunc>([&db, &flags](auto&& e) {
        fl::api(sqlite3_create_function_v2,
                { SQLITE_OK },
                db.ptr().get(),
                db.ptr().get(),
                Derived::name,
                e.value, // argc
                flags,
                static_cast<void*>(new Derived),
                make_scalar_lambda<&Derived::xFunc>(
                  std::make_index_sequence<e.value>()),
                nullptr,
                nullptr,
                [](void* ptr) {
                  auto impl_ptr = static_cast<Derived*>(ptr);
                  delete impl_ptr;
                });
      });
    }

    if constexpr (!boost::mp11::mp_empty<sig_xinverse>::value) {

      // window

      static_assert(std::is_same_v<sig_xstep, sig_xinverse>);

      boost::mp11::mp_for_each<sig_xstep>([&db, &flags](auto&& e) {
        fl::api(
          sqlite3_create_window_function,
          { SQLITE_OK },
          db.ptr().get(),
          db.ptr().get(),
          Derived::name,
          e.value, // argc
          flags,
          nullptr,
          make_values_lambda<&Derived::xStep, callback_selector::xStep>(
            std::make_index_sequence<e.value>()),
          make_simple_lambda<callback_selector::xFinal>(),
          make_simple_lambda<callback_selector::xValue>(),
          make_values_lambda<&Derived::xInverse, callback_selector::xInverse>(
            std::make_index_sequence<e.value>()),
          nullptr);
      });

    } else if constexpr (!boost::mp11::mp_empty<sig_xstep>::value) {

      // aggregate

      boost::mp11::mp_for_each<sig_xstep>([&db, &flags](auto&& e) {
        fl::api(sqlite3_create_function_v2,
                { SQLITE_OK },
                db.ptr().get(),
                db.ptr().get(),
                Derived::name,
                e.value, // argc
                flags,
                nullptr,
                nullptr,
                make_values_lambda<&Derived::xStep, callback_selector::xStep>(
                  std::make_index_sequence<e.value>()),
                make_simple_lambda<callback_selector::xFinal>(),
                nullptr);
      });
    }
  }
};
}

#endif
