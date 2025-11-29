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

from objects.objects import JsonObject
from objects.errors import ValidationException
import re

class RequestObject(JsonObject):

    def __init__(self, source: dict[str, str]={}):
        super().__init__(source)

    def validate(self):
        pass

class LoginRequest(RequestObject):

    def __init__(self, source: dict[str, str]={}):
        super().__init__(source)

    @property
    def username(self) -> str:
        return self.get_prop("username")

    @property
    def password(self) -> str:
        return self.get_prop("password")

    def validate(self):
        if not self.username:
            raise ValidationException("Missing username")
        if not self.password:
            raise ValidationException("Missing password")

class CreateAccountRequest(RequestObject):

    REGEX_EMAIL_ADDRESS = re.compile(
        r"(?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|\"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*\")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\[(?:(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9]))\.){3}(?:(2(5[0-5]|[0-4][0-9])|1[0-9][0-9]|[1-9]?[0-9])|[a-z0-9-]*[a-z0-9]:(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21-\x5a\x53-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])+)\])"
    )

    def __init__(self, source: dict[str, str]={}):
        super().__init__(source)

    @property
    def username(self) -> str:
        return self.get_prop("username")

    @property
    def email_address(self) -> str:
        return self.get_prop("email_address")

    @property
    def password(self) -> str:
        return self.get_prop("password")

    def validate(self):
        if not self.username:
            raise ValidationException("Missing username")
        if not self.email_address:
            raise ValidationException("Missing email address")
        if not self.__class__.REGEX_EMAIL_ADDRESS.fullmatch(self.email_address):
            raise ValidationException("Invalid email address")
        if not self.password:
            raise ValidationException("Missing password")
        if len(self.password) < 8:
            raise ValidationException("Passwords should contain at least 8 characters")
        if self.get_prop("confirm_password") != self.password:
            raise ValidationException("Passwords don't match")
