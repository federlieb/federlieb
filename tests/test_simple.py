from rich import print
from rich.traceback import install as rich_install

import sys
import rich
import pysqlite3 as sqlite3
import sqlite3 as sq
import pytest
import networkx
import json
import warnings
import random
from typing import TypeAlias

Cursor: TypeAlias = sq.Cursor
Db: TypeAlias = sq.Connection

rich_install(show_locals=True)


def quote_string(s: str) -> str:
    return f"""'{ s.replace("'", "''") }'"""


@pytest.fixture
def db() -> Db:
    db = sqlite3.connect(":memory:")
    db.enable_load_extension(True)
    db.load_extension("build/fl_extensions")
    db.enable_load_extension(False)
    return db


def test_nameless_simple(db: Db):
    cur: Cursor = db.cursor()

    cur.execute(
        """
        select name from pragma_module_list order by 1
    """
    )

    direct = cur.fetchall()

    cur.execute(
        """
        select c1 from fl_nameless('pragma_module_list') order by 1
    """
    )

    indirect = cur.fetchall()

    assert direct == indirect


def test_stmt_cache(db: Db):
    cur = db.cursor()
    cur.execute(
        """
        CREATE VIRTUAL TABLE t USING fl_stmt((
            SELECT RANDOMBLOB(16) AS rand
        ), key=(SELECT 1))
    """
    )

    cur.execute("SELECT * FROM t")
    first = cur.fetchall()
    cur.execute("SELECT * FROM t")
    second = cur.fetchall()

    assert first == second


def test_stmt_nocache(db: Db):
    cur = db.cursor()
    cur.execute(
        """
        CREATE VIRTUAL TABLE t USING fl_stmt((
            SELECT RANDOMBLOB(16) AS rand
        ), key=(SELECT RANDOMBLOB(16)))
    """
    )

    cur.execute("SELECT * FROM t")
    first = cur.fetchall()
    cur.execute("SELECT * FROM t")
    second = cur.fetchall()

    assert first != second


def test_dominator_tree(db: Db):
    count_vertices = 100
    count_edges = 3 * count_vertices

    g = networkx.generators.gnm_random_graph(count_vertices, count_edges, directed=True)
    root = random.choice(list(g.nodes))
    pairs = networkx.immediate_dominators(g, root).items()

    cur: Cursor = db.cursor()

    cur.execute(
        """
        DROP TABLE IF EXISTS d
    """
    )

    cur.execute(
        f"""
        CREATE VIRTUAL TABLE d USING fl_dominator_tree(
            edges=(
                SELECT
                    value->>'$[0]',
                    value->>'$[1]'
                FROM
                    JSON_EACH({
                        quote_string(json.dumps(list(g.edges)))
                    })
            )
        );
    """
    )

    cur.execute("SELECT * FROM d WHERE root = 0 + :root", {"root": root})

    result = cur.fetchall()

    rich.print(f"root: {root}")
    rich.print(g.edges)
    rich.print(sorted(pairs))
    rich.print(sorted(result))

    assert set(result) == set([x for x in pairs if x[0] != x[1]])
