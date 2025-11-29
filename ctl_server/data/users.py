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

import bcrypt
import data.common as dc
import logging
from datetime import datetime
from sqlite3 import IntegrityError
from data.entity import Entity
from data.errors import DataException

class User(Entity):

    def __init__(self, source: dict={}):
        super().__init__(source)

    @property
    def username(self) -> str:
        return self.get_prop("username")

    @username.setter
    def username(self, val: str):
        self.set_prop("username", val)

    @property
    def hashed_password(self) -> str:
        return self.get_prop("hashed_password")

    @hashed_password.setter
    def hashed_password(self, val: str):
        self.set_prop("hashed_password", val)

    @property
    def email_address(self) -> str:
        return self.get_prop("email_address")

    @email_address.setter
    def email_address(self, val: str):
        self.set_prop("email_address", val)

    @property
    def created(self) -> int:
        return self.get_prop("created")

    @created.setter
    def created(self, val: int):
        self.set_prop("created", val)

    def set_hashed_password(self, plaintext: str, salt: bytes):
        self.hashed_password = bcrypt.hashpw(plaintext.encode(), salt).hex()

    def plaintext_matching_stored(self, plaintext: str) -> bool:
        return bcrypt.checkpw(plaintext.encode(), bytes.fromhex(self.hashed_password))


def create_user(username, email_address, password_plaintext):
    user = User({
        "username": username,
        "email_address": email_address,
        "created": datetime.now().timestamp(),
    })
    user.set_hashed_password(password_plaintext, bcrypt.gensalt())
    try:
        user.id = dc.run_query("""
            INSERT INTO users(
                        username,
                        email_address,
                        hashed_password,
                        created
                        )
                 VALUES (
                        ?,
                        ?,
                        ?,
                        ?
                        )
        """, (
            user.username,
            user.email_address,
            user.hashed_password,
            user.created,
            )
        )
    except IntegrityError as e:
        logging.exception(e)
        raise DataException("Duplicate username or password")

    user.hashed_password = None
    return user

def authenticate_user(username, password_plaintext):
    user_row = dc.query_db("""
        SELECT *
          FROM users
         WHERE username = ?
    """, (username, ), one=True)

    if not user_row:
        raise DataException("Username incorrect")

    user = User(user_row)
    if not user.plaintext_matching_stored(password_plaintext):
        raise DataException("Password incorrect")

    user.hashed_password = None
    return user

def fetch_user_by_id(user_id):
    user_row = dc.query_db("""
        SELECT *
          FROM users
         WHERE id = ?
    """, (user_id, ), one=True)

    if not user_row:
        raise DataException("User not found")

    user = User(user_row)
    user.hashed_password = None
    return user
