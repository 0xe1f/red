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

class Entity:

    def __init__(self, source: dict={}):
        self._doc = source.copy()

    def __eq__(self, value: object) -> bool:
        return self._doc == value._doc

    @property
    def id(self) -> str:
        return self.get_prop("id")

    @id.setter
    def id(self, val: str):
        self.set_prop("id", val)

    def get_prop(self, name: str) -> object:
        return self._doc.get(name)

    def set_prop(self, name: str, val: object|None):
        if val != None:
            self._doc[name] = val
        else:
            # Delete null values
            self._doc.pop(name, None)
