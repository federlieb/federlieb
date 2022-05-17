# federlieb - Modern C++23 SQLite language bindings

There are a number of free and open source C++ libraries wrapping the
SQLite C API with a more C++ friendly interface. Unfortunately, most
of them are incomplete and unmaintained, and this one is no different.

You are, however, very welcome to contribute to this one.

The original author's motivation in developing this library was making
SQLite extension functions and virtual tables easier to write, and 
experimenting with modern C++ features. They have a profound dislike
of macros, workarounds for compiler limitations, silent data loss, and
code duplication.

Some of the motivating extensions are currently part of this repository,
as exposition and test cases. They are likely to be moved to some other
repository in the future.

## Open issues

### Handling sqlite3 values

I am not happy yet with how values are hanled throughout the library.
Different parts of it use different approaches, for experimentation and
due to slightly different requirements. Those should be improved and be
consolidated.

### Virtual Table API

This is pending ongoing experimentation.

## Highlight: reading results into structures automatically

With help from Boost's »pfr« library, _federlieb_ can automatically
render query results into structures for convenient and safe access:

```c++

struct example_data {
  uint32_t id;
  std::string name;
  double value;
};

auto stmt = db.prepare("SELECT id, name, value FROM example").execute();

for (auto row : stmt | fl::as<example_data>()) {
  std::cout << row.name << '\n';
}
```

## Highlight: easy extension functions

The _federlieb_ library makes it easy to define extension functions by
way of CRTP classes. The following is a simple aggregate function that
computes a floating point approximation of the sum of a set of values:

```c++
class total_aggregate : public fl::fx::base<total_aggregate>
{
public:
  static constexpr auto name = "total";
  static constexpr auto deterministic = true;
  static constexpr auto direct_only = false;

  void xStep(const double& value) {
    accumulator += value;
  }

  double xFinal() {
    return accumulator;
  }

protected:
  double accumulator = 0.0f;
};
```

The library automatically figures out whether a class represents a
scalar, aggregate, or window function, how many arguments it takes,
including support for C++ default values, automatically converts
SQLite values to the desired C++ data types and vice-versa, and 
manages the lifetime of the function and its resources. You can write:

```c++
class ex_scalar_function : public fl::fx::base<ex_scalar_function> {
...
uint32_t xFunc(const std::string& s, int radix = 10) { ... }
...
}
```

and the library will present this to SQLite as scalar function that
can take one or two parameters, the second parameter will be set to
the specified default value when the function is called with only a
single argument in SQL, and when the second argument is larger than
what an `int` can hold, an error will be communicated to SQLite, with
no additional effort required on part of the extension function author.

## Highlight: range-friendly

...

## Highlight: easy virtual tables

...

```c++
class example_vt : public fl::vtab::base<example_vt>
{
public:
  static constexpr auto name = "example_vt";

  struct cursor
  {
    cursor(example_vt* vtab) {}
  };

  void xConnect(bool) {
    declare(R"SQL(

      CREATE TABLE example_vt(
        input INT VT_REQUIRED,
        output INT
      )

    )SQL");
  }

  fl::stmt xFilter(const fl::vtab::index_info& info, cursor* cursor) {
    auto tmp = fl::db(":memory:");
    auto stmt = tmp.prepare("SELECT :input, :input + 3");
    stmt.execute(info.get("input", SQLITE_INDEX_CONSTRAINT_EQ));
    return stmt;
  }
};
```

## Virtual table `vt_stmt`

```sql
CREATE VIRTUAL TABLE cached_query USING fl_stmt((

  SELECT 123 AS value, :param AS input_parameter

), key=(SELECT total_changes()))
```

## Virtual table `vt_nameless`

## Virtual table `vt_contraction`

## Virtual table `vt_partition_by`

## Virtual table `vt_dominator_tree`
## Virtual table `vt_transitive_closure`
## Virtual table `vt_strong_components`
## Virtual table `vt_weak_components`

## Extension functions `fl_toset*`

```c++
// JSON array of ordered `input` elements without duplicates (shallow)
json_array fl_toset(json_array input);

// fl_toset(JSON_GROUP_ARRAY(...))
json_array fl_toset_agg(json_value);

// set union
json_array fl_toset_union(json_array lhs, json_array rhs);

// set difference
json_array fl_toset_except(json_array lhs, json_array rhs);

// set intersection
json_array fl_toset_intersection(json_array lhs, json_array rhs);

// shallow membership test
bool fl_toset_contains(json_array haystack, json_value needle);
```

These functions deal with the moral equivalent of

```sql
SELECT JSON_GROUP_ARRAY(DISTINCT value ORDER BY value) ...
```

... which is unfortunately not supported by SQLite as of v3.38. That
is, various functions that treat JSON arrays as ordered sets without
duplicates based on a shallow total order. Shallow here means that
nested arrays or objects are compared as-is, so

```sql
SELECT fl_toset('[{"a":1,"b":2},{"b":2,"a":1}]')
```

...

## Extension function `fl_sha1`

Using the Linux Kernel Crypto userland API, the `fl_sha1` function
computes the SHA-1 sum of a blob.

```c++
blob fl_sha1(blob data)
```

Use the `HEX` function built into SQLite to turn the result into a
hexadecimal string. Other functions provided by the Linux Kernel can
similarily be added with a couple of lines of code.

## Extension function `fl_counter`

The `fl_counter` function maintains a mapping between a string key and an
integer counter value that can be queried, incremented, and decremented.

```c++
int64_t fl_counter(std::string key, int64_t diff = 0)
```

This can be useful to track changes in tables using triggers:

```sql
CREATE TRIGGER trigger AFTER INSERT ON table
BEGIN SELECT fl_counter('table_inserts', +1); END;
```

The implementation will forget a key when the value is reset to zero:

```sql
SELECT fl_counter('key', -fl_counter('key'))
```

Use this as an alternative to `total_changes()` et al. as more granular
cache key for other extensions like `fl_stmt`.

## Extension function `fl_ordered_concat_agg`

Aggregate function that orderes values by a sort key and returns a string
that joines all values with a separator. As if:

```sql
SELECT GROUP_CONCAT(value ORDER BY sort_key, ',')
```

... which is not supported by SQLite as of v3.38. Like `GROUP_CONCAT`,
the separator can be different for each value. The separator for the
(in the resulting order) first element is ignored. The order is stable
(but SQLite does not guarantee any particular order in which aggregate
functions are called), and all `NULL` values are considered equal.
