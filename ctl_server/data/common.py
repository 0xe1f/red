# Copyright (C) 2025 Akop Karapetyan
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __main__ import app
from flask import g
from secrets import token_urlsafe
import sqlite3

def get_db():
    db = getattr(g, "_database", None)
    if db is None:
        db = g._database = sqlite3.connect(app.config["DATABASE_NAME"])
        db.row_factory = make_dicts
        init_db()

    return db

def make_dicts(cursor, row):
    return dict((cursor.description[idx][0], value)
                for idx, value in enumerate(row))

def run_query(query, args=(), autocommit=True):
    db = get_db()
    cur = db.execute(query, args)
    cur.close()
    if autocommit:
        db.commit()
    return cur.lastrowid

def query_db(query, args=(), one=False):
    cur = get_db().execute(query, args)
    rv = cur.fetchall()
    cur.close()
    return (rv[0] if rv else None) if one else rv

@app.teardown_appcontext
def close_connection(_):
    db = getattr(g, "_database", None)
    if db is not None:
        db.close()

def generate_pubid():
    return token_urlsafe(nbytes=8)

def init_db():
    run_query("""
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            username TEXT NOT NULL,
            email_address TEXT NOT NULL,
            hashed_password TEXT NOT NULL,
            created INTEGER NOT NULL
        )
    """, autocommit=False)
    run_query("""
        CREATE UNIQUE INDEX IF NOT EXISTS users_username ON users (username)
    """, autocommit=False)
    run_query("""
        CREATE UNIQUE INDEX IF NOT EXISTS users_email_address ON users (email_address)
    """, autocommit=False)
    run_query("""
        CREATE TABLE IF NOT EXISTS sessions (
            id INTEGER PRIMARY KEY,
            user_id INTEGER NOT NULL,
            client_addr TEXT NOT NULL,
            created INTEGER NOT NULL
        )
    """, autocommit=False)
    run_query("""
        CREATE INDEX IF NOT EXISTS sessions_user_id ON sessions (user_id)
    """, autocommit=False)
    get_db().commit()
