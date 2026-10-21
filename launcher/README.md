Launcher
===
Launcher is the web UI component for red. It's responsible for displaying
a list of available titles, and running queries and requests via the
[remote](../remote/). Actual communication with Remote is over NATS.

Video window
---
The **Video** button in the header toggles a floating window that shows the
game's video. While it is open, the browser reads `GET /video/stream` (login
required), and the launcher relays the frames it receives on the
`red.video.frames` NATS subject. Only the launcher needs to reach NATS; the
browser needs nothing beyond the launcher itself.

The launcher subscribes to NATS only while at least one window is open, and
shares that subscription between viewers. A viewer that can't keep up skips
frames rather than falling behind.

The stream is a sequence of messages, each a little-endian `u32` length
followed by that many bytes of frame, exactly as published (see
`pubsub/vid_frame.h`). A length of zero is a keepalive. Decoding (LZ4 and pixel
conversion) happens in the browser, in `static/js/video.js`.
