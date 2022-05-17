from rich import print
import pysqlite3 as sqlite3
import pytest


@pytest.fixture
def db():
    db = sqlite3.connect(':memory:')
    db.enable_load_extension(True)
    db.load_extension('build/fl_extensions')
    db.enable_load_extension(False)
    return db


def test_nameless_simple(db):
    cur = db.cursor()

    cur.execute("""
        select name from pragma_module_list order by 1
    """)

    direct = cur.fetchall()

    cur.execute("""
        select c1 from fl_nameless('pragma_module_list') order by 1
    """)

    indirect = cur.fetchall()

    assert direct == indirect


def test_stmt_cache(db):
    cur = db.cursor()
    cur.execute("""
        CREATE VIRTUAL TABLE t USING fl_stmt((
            SELECT RANDOMBLOB(16) AS rand
        ), key=(SELECT 1))
    """)

    cur.execute("SELECT * FROM t")
    first = cur.fetchall()
    cur.execute("SELECT * FROM t")
    second = cur.fetchall()

    assert first == second


def test_stmt_nocache(db):
    cur = db.cursor()
    cur.execute("""
        CREATE VIRTUAL TABLE t USING fl_stmt((
            SELECT RANDOMBLOB(16) AS rand
        ), key=(SELECT RANDOMBLOB(16)))
    """)

    cur.execute("SELECT * FROM t")
    first = cur.fetchall()
    cur.execute("SELECT * FROM t")
    second = cur.fetchall()

    assert first != second
