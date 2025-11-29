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

import data.common as dc
from datetime import datetime
from data.entity import Entity

class Session(Entity):

    def __init__(self, source: dict={}):
        super().__init__(source)

    @property
    def user_id(self) -> str:
        return self.get_prop("user_id")

    @user_id.setter
    def user_id(self, val: str):
        self.set_prop("user_id", val)

    @property
    def client_addr(self) -> str:
        return self.get_prop("client_addr")

    @client_addr.setter
    def client_addr(self, val: str):
        self.set_prop("client_addr", val)

    @property
    def created(self) -> int:
        return self.get_prop("created")

    @created.setter
    def created(self, val: int):
        self.set_prop("created", val)


def create_session(user_id, client_addr):
    session = Session({
        "user_id": user_id,
        "client_addr": client_addr,
        "created": datetime.now().timestamp(),
    })
    session.id = dc.run_query("""
        INSERT INTO sessions(
                    user_id,
                    client_addr,
                    created
                    )
             VALUES (
                    ?,
                    ?,
                    ?
                    )
    """, (
        session.user_id,
        session.client_addr,
        session.created,
        )
    )

    return session
