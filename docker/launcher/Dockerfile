## Copyright (c) 2024 Akop Karapetyan
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##    http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.

FROM python:3.14-slim

RUN apt update -y && \
    apt install -y \
        git \
        cron \
        build-essential \
        tini

ARG HTTP_PORT
ARG GIT_REPO
ARG GIT_BRANCH
ARG ETC_PATH=/etc/red

COPY launcher /opt/red
# RUN git clone --branch $GIT_BRANCH $GIT_REPO /opt/red

COPY etc/boot.sh $ETC_PATH/boot.sh

WORKDIR /opt/red
COPY generated/config.yaml config.yaml
COPY generated/games.yaml games.yaml

RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

RUN pip install -r requirements.txt
RUN pip install gunicorn eventlet
RUN chmod a+x $ETC_PATH/boot.sh

ENV FLASK_APP=launcher.py
ENV PRODUCTION=true
ENV HTTP_PORT=$HTTP_PORT

EXPOSE $HTTP_PORT

ENTRYPOINT [ "tini", "--", "/etc/red/boot.sh" ]
