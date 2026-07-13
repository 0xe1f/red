Pubsub
===
This is the primary emulation driver and related video renderer. It's
implemented as two distinct, but interdependent components -

* pub is the primary emulation driver - a custom
  [libretro](https://www.libretro.com/) host that publishes video frames to
  NATS. Audio and input are rendered and consumed by pub and are not published.
  Pub is executed as a standard process and killed when no longer needed.
* sub is a long-running service that subscribes to the published video frames
  and displays them on an LED matrix of some fixed size and configuration

Frames are always published in a specific fixed size by the publisher;
subscribers get to decide how much of each frame they will utilize. All scaling,
filtering and compression is done pre-publish.

## Requirements

```
sudo apt install nats-server libprotobuf-c-dev libnats-dev protobuf-c-compiler
```

## License

   Copyright 2024, Akop Karapetyan

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
