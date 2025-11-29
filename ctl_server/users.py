# Copyright (C) 2024-2025 Akop Karapetyan
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
from data.errors import DataException
from flask_login import current_user
from objects.errors import ValidationException
import data.sessions as ds
import data.users as du
import flask
import flask_login
import logging
import objects.requests as requests
import objects.objects as objects
import re

login_manager = flask_login.LoginManager()
login_manager.login_view = "/login"
login_manager.init_app(app)

REGEX_REDIRECT_URL = re.compile(r"^(/\w+)+|/$")

@login_manager.user_loader
def load_user(user_id):
    try:
        return objects.User(du.fetch_user_by_id(user_id))
    except DataException as e:
        app.logger.error(e.message)
        return None

@app.get("/login")
def login_get():
    if current_user.is_authenticated:
        return flask.redirect(flask.url_for("index"))

    return flask.render_template(
        "login.html",
    )

@app.post("/login")
def login_post():
    arg = requests.LoginRequest(flask.request.form)

    try:
        arg.validate()
    except ValidationException as e:
        app.logger.error(e.message)
        flask.flash(e.message)
        return flask.render_template(
            "login.html",
            arg=arg,
        )

    try:
        user = du.authenticate_user(
            arg.username,
            arg.password
        )
    except DataException as e:
        app.logger.error(e.message)
        flask.flash("Username or password incorrect")
        return flask.render_template(
            "login.html",
            arg=arg,
        )

    try:
        session = ds.create_session(user.id, flask.request.remote_addr)
        app.logger.info(f"Session ({session.id}) created")
    except DataException as e:
        app.logger.error(e.message)
        flask.flash("Error logging in")
        return flask.render_template(
            "login.html",
            arg=arg,
        )

    flask.session.permanent = True
    flask_login.utils.login_user(objects.User(user))

    if next := flask.request.args.get("next"):
        if not REGEX_REDIRECT_URL.fullmatch(next):
            logging.warning(f"{next} is not a valid redirection URL")
            next = None

    return flask.redirect(next or flask.url_for("index"))

@app.get("/logout")
@flask_login.login_required
def logout():
    flask_login.logout_user()
    return flask.redirect(flask.url_for("login_get"))

@app.get("/create_account")
def create_account_get():
    if current_user.is_authenticated:
        return flask.redirect(flask.url_for("index"))

    return flask.render_template(
        "create_account.html",
    )

@app.post("/create_account")
def create_account_post():
    arg = requests.CreateAccountRequest(flask.request.form)

    try:
        arg.validate()
    except ValidationException as e:
        app.logger.error(e.message)
        flask.flash(e.message)
        return flask.render_template(
            "create_account.html",
            arg=arg,
        )

    try:
        user = du.create_user(
            arg.username,
            arg.email_address,
            arg.password,
        )
    except DataException as e:
        app.logger.error(e.message)
        flask.flash("Duplicate username or email address")
        return flask.render_template(
            "create_account.html",
            arg=arg,
        )

    flask.flash(f"Account '{user.username}' created successfully", "success")
    return flask.redirect(flask.url_for("login_get"))
