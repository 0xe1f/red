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

import data.users as du

class JsonObject:

    def __init__(self, source: dict={}):
        self._doc = { k:v for k, v in source.items() if v != None }

    def get_prop(self, name: str, default: object=None) -> object:
        return self._doc.get(name, default)

    def set_prop(self, name: str, val: object):
        if val != None:
            self._doc[name] = val
        else:
            # Delete null values
            self._doc.pop(name, None)

    def __bool__(self):
        return not not self._doc

    def as_dict(self):
        return self._doc

class User(JsonObject):

    def __init__(self, user: du.User|None=None, source: dict={}):
        super().__init__(source)
        if user:
            self.set_prop("id", user.id)
            self.set_prop("username", user.username)
            self.set_prop("email_address", user.email_address)

    @property
    def id(self) -> str:
        return self._doc.get("id")

    @property
    def username(self) -> str:
        return self._doc.get("username")

    @property
    def email_address(self) -> str:
        return self._doc.get("email_address")

    @property
    def is_authenticated(self) -> str:
        return not not self.id

    @property
    def is_active(self) -> str:
        return self.is_authenticated

    @property
    def is_anonymous(self) -> str:
        return not self.is_authenticated

    def get_id(self):
        return self._doc.get("id")
