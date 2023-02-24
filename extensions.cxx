extern "C"
{
#include "sqlite3ext.h"

  SQLITE_EXTENSION_INIT1
};

#include "federlieb/federlieb.hxx"

#include "ext/fx_counter.hxx"
#include "ext/fx_kcrypto.hxx"
#include "ext/fx_ordered_concat_agg.hxx"
#include "ext/fx_toset.hxx"

#include "ext/vt_contraction.hxx"
#include "ext/vt_dominator_tree.hxx"
#include "ext/vt_nameless.hxx"
#include "ext/vt_partition_by.hxx"
#include "ext/vt_stmt.hxx"
#include "ext/vt_strong_components.hxx"
#include "ext/vt_transitive_closure.hxx"
#include "ext/vt_weak_components.hxx"
#include "ext/vt_dijkstra_shortest_paths.hxx"

#include "ext/vt_json_each.hxx"
#include "ext/vt_script.hxx"
#include "ext/vt_dfa.hxx"

extern "C"
{
#ifdef _WIN32
  __declspec(dllexport)
#endif
    int fl_nameless_init(sqlite3* db,
                         char** pzErrMsg,
                         const sqlite3_api_routines* pApi) noexcept
  {

    SQLITE_EXTENSION_INIT2(pApi);

    try {
      auto ours = federlieb::db(std::shared_ptr<sqlite3>(db, [](auto) {}));

      vt_dominator_tree::register_module(ours);
      vt_stmt::register_module(ours);
      vt_partition_by::register_module(ours);
      vt_strong_components::register_module(ours);
      vt_weak_components::register_module(ours);
      vt_transitive_closure::register_module(ours);
      vt_nameless::register_module(ours);
      vt_contraction::register_module(ours);
      vt_dijkstra_shortest_paths::register_module(ours);
      vt_json_each::register_module(ours);
      vt_script::register_module(ours);
      vt_dfa::register_module(ours);

      fx_toset::register_function(ours);
      fx_toset_agg::register_function(ours);
      fx_toset_union::register_function(ours);
      fx_toset_intersection::register_function(ours);
      fx_toset_except::register_function(ours);
      fx_toset_contains::register_function(ours);
      fx_object_set_agg::register_function(ours);

      fx_stack_push::register_function(ours);
      fx_stack_pop::register_function(ours);
      fx_stack_top::register_function(ours);

      fx_sha1::register_function(ours);

      fx_counter::register_function(ours);

      fx_ordered_concat_agg::register_function(ours);

    } catch (...) {
      return SQLITE_ERROR;
    }

    return SQLITE_OK;
  }

#ifdef _WIN32
  __declspec(dllexport)
#endif
    int sqlite3_extension_init(sqlite3* db,
                               char** pzErrMsg,
                               const sqlite3_api_routines* pApi) noexcept
  {
    int rc = SQLITE_OK;

    rc = fl_nameless_init(db, pzErrMsg, pApi);

    return rc;
  }
};
