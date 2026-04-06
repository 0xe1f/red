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
import logging
from datetime import datetime
from data.entity import Entity

class Launch(Entity):

    def __init__(self, source: dict={}):
        super().__init__(source)

    @property
    def session_id(self) -> int:
        return self.get_prop("session_id")

    @session_id.setter
    def session_id(self, val: int):
        self.set_prop("session_id", val)

    @property
    def uid(self) -> str:
        return self.get_prop("uid")

    @uid.setter
    def uid(self, val: str):
        self.set_prop("uid", val)

    @property
    def launched_at(self) -> int:
        return self.get_prop("launched_at")

    @launched_at.setter
    def launched_at(self, val: int):
        self.set_prop("launched_at", val)

    @property
    def stopped_at(self) -> int | None:
        return self.get_prop("stopped_at")

    @stopped_at.setter
    def stopped_at(self, val: int | None):
        self.set_prop("stopped_at", val)


def create_launch(session_id: int, uid: str) -> Launch:
    launch = Launch({
        "session_id": session_id,
        "uid": uid,
        "launched_at": datetime.now().timestamp(),
    })
    launch.id = dc.run_query("""
        INSERT INTO launches(
                    session_id,
                    uid,
                    launched_at
                    )
             VALUES (
                    ?,
                    ?,
                    ?
                    )
    """, (
        launch.session_id,
        launch.uid,
        launch.launched_at,
        )
    )

    return launch

def end_launch():
    launch = fetch_latest_launch()
    if not launch or launch.stopped_at:
        logging.warning("Unexpected launch state")
        return

    launch.stopped_at = datetime.now().timestamp()
    dc.run_query("""
        UPDATE launches
           SET stopped_at = ?
         WHERE id = ?
    """, (
        launch.stopped_at,
        launch.id,
        )
    )

def fetch_latest_launch() -> Launch | None:
    launch_row = dc.query_db("""
        SELECT *
          FROM launches
         ORDER BY launched_at DESC
         LIMIT 1
    """, (), one=True)

    if not launch_row:
        return None

    launch = Launch(launch_row)
    return launch

def increment_launch_count(uid: str):
    dc.run_query("""
        INSERT INTO launch_counts (
                    uid,
                    count
                    )
             VALUES (
                    ?,
                    1
                    )
        ON CONFLICT (
                    uid
                    )
          DO UPDATE
                SET count = count + 1
    """, (
        uid,
        )
    )
