/*****************************************************************************
 ** Copyright (C) 2026 Akop Karapetyan
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 ******************************************************************************
**/

// Receives video frames relayed by the launcher (GET /video/stream) and
// decodes them for display.
//
// The stream is a sequence of messages, each a little-endian u32 length
// followed by that many bytes; a length of zero is a keepalive. A message is a
// frame exactly as published on NATS (see pubsub/vid_frame.h):
//
//   pitch(u16) width(u16) height(u16) pixel_format(u8) attrs(u8)   (LE)
//   followed by an LZ4 block that expands to pitch * height bytes.

export const PF_RGB565   = 1;
export const PF_RGBA8888 = 2;
export const PF_RGBA5551 = 3;
export const PF_ARGB8888 = 4;

export const ATTR_ROT180 = 0x01;

const HEADER_SIZE = 8;

// Decompresses a raw LZ4 block from src (starting at srcStart) into dst.
// Returns the number of bytes written.
export function lz4Decompress(src, srcStart, dst) {
    const srcEnd = src.length;
    const dstEnd = dst.length;
    let s = srcStart;
    let d = 0;
    while (s < srcEnd) {
        const token = src[s++];

        let litLen = token >> 4;
        if (litLen === 15) {
            let b;
            do {
                if (s >= srcEnd) throw new Error("lz4: truncated literal length");
                b = src[s++];
                litLen += b;
            } while (b === 255);
        }
        if (s + litLen > srcEnd || d + litLen > dstEnd) {
            throw new Error("lz4: literal run out of bounds");
        }
        dst.set(src.subarray(s, s + litLen), d);
        s += litLen;
        d += litLen;
        if (s >= srcEnd) {
            break; // last sequence has literals only
        }

        if (s + 2 > srcEnd) throw new Error("lz4: truncated offset");
        const offset = src[s] | (src[s + 1] << 8);
        s += 2;
        if (offset === 0 || offset > d) throw new Error("lz4: bad match offset");

        let matchLen = token & 15;
        if (matchLen === 15) {
            let b;
            do {
                if (s >= srcEnd) throw new Error("lz4: truncated match length");
                b = src[s++];
                matchLen += b;
            } while (b === 255);
        }
        matchLen += 4;
        if (d + matchLen > dstEnd) throw new Error("lz4: match out of bounds");

        let from = d - offset;
        if (offset >= matchLen) {
            dst.copyWithin(d, from, from + matchLen);
            d += matchLen;
        } else {
            // Overlapping match: must replicate byte by byte
            for (let i = 0; i < matchLen; i++) {
                dst[d++] = dst[from++];
            }
        }
    }
    return d;
}

// Decodes a published frame message into { width, height, rgba }, where rgba
// is a Uint8ClampedArray suitable for ImageData. attrs are applied, so the
// result is what should be shown on screen.
export function decodeFrame(data) {
    if (data.length < HEADER_SIZE) {
        throw new Error("frame: message too short");
    }
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);
    const pitch = view.getUint16(0, true);
    const width = view.getUint16(2, true);
    const height = view.getUint16(4, true);
    const format = view.getUint8(6);
    const attrs = view.getUint8(7);
    if (!width || !height) {
        throw new Error("frame: empty dimensions");
    }

    const pixels = new Uint8Array(pitch * height);
    const size = lz4Decompress(data, HEADER_SIZE, pixels);
    if (size !== pixels.length) {
        throw new Error(`frame: expected ${pixels.length} bytes, got ${size}`);
    }

    const rgba = new Uint8ClampedArray(width * height * 4);
    const out = new Uint32Array(rgba.buffer); // little-endian: A B G R
    switch (format) {
    case PF_ARGB8888:
        // XRGB8888 in little-endian memory: bytes are B, G, R, X
        for (let y = 0; y < height; y++) {
            const row = y * pitch;
            for (let x = 0; x < width; x++) {
                const i = row + x * 4;
                out[y * width + x] = 0xff000000
                    | (pixels[i] << 16) | (pixels[i + 1] << 8) | pixels[i + 2];
            }
        }
        break;
    case PF_RGBA8888:
        // Bytes are R, G, B, X
        for (let y = 0; y < height; y++) {
            const row = y * pitch;
            for (let x = 0; x < width; x++) {
                const i = row + x * 4;
                out[y * width + x] = 0xff000000
                    | (pixels[i + 2] << 16) | (pixels[i + 1] << 8) | pixels[i];
            }
        }
        break;
    case PF_RGB565:
        for (let y = 0; y < height; y++) {
            const row = y * pitch;
            for (let x = 0; x < width; x++) {
                const c = pixels[row + x * 2] | (pixels[row + x * 2 + 1] << 8);
                const r = (((c >> 11) & 0x1f) * 255 / 31) | 0;
                const g = (((c >> 5) & 0x3f) * 255 / 63) | 0;
                const b = ((c & 0x1f) * 255 / 31) | 0;
                out[y * width + x] = 0xff000000 | (b << 16) | (g << 8) | r;
            }
        }
        break;
    case PF_RGBA5551:
        for (let y = 0; y < height; y++) {
            const row = y * pitch;
            for (let x = 0; x < width; x++) {
                const c = pixels[row + x * 2] | (pixels[row + x * 2 + 1] << 8);
                const r = (((c >> 11) & 0x1f) * 255 / 31) | 0;
                const g = (((c >> 6) & 0x1f) * 255 / 31) | 0;
                const b = (((c >> 1) & 0x1f) * 255 / 31) | 0;
                out[y * width + x] = 0xff000000 | (b << 16) | (g << 8) | r;
            }
        }
        break;
    default:
        throw new Error(`frame: unsupported pixel format ${format}`);
    }

    if (attrs & ATTR_ROT180) {
        out.reverse(); // reversing the pixel order is a 180-degree rotation
    }
    return { width, height, rgba };
}

// Splits the length-prefixed byte stream back into messages, however the
// network happened to chunk it.
export class MessageReader {
    constructor() {
        this.buf = new Uint8Array(64 * 1024);
        this.len = 0;
    }

    // Appends a chunk and returns the complete messages now available
    // (keepalives are dropped).
    push(chunk) {
        if (this.len + chunk.length > this.buf.length) {
            let size = this.buf.length;
            while (size < this.len + chunk.length) {
                size *= 2;
            }
            const grown = new Uint8Array(size);
            grown.set(this.buf.subarray(0, this.len));
            this.buf = grown;
        }
        this.buf.set(chunk, this.len);
        this.len += chunk.length;

        const messages = [];
        let pos = 0;
        while (this.len - pos >= 4) {
            const n = (this.buf[pos] | (this.buf[pos + 1] << 8)
                | (this.buf[pos + 2] << 16) | (this.buf[pos + 3] << 24)) >>> 0;
            if (n === 0) {
                pos += 4;
                continue;
            }
            if (this.len - pos - 4 < n) {
                break;
            }
            messages.push(this.buf.slice(pos + 4, pos + 4 + n));
            pos += 4 + n;
        }
        if (pos > 0) {
            this.buf.copyWithin(0, pos, this.len);
            this.len -= pos;
        }
        return messages;
    }
}

// Streams video frames from the launcher.
//
//   const stream = new VideoStream({
//       url: "/video/stream",
//       onFrame: ({ width, height, rgba }) => { ... },
//       onStatus: (state, detail) => { ... },
//   });
//   stream.start();
//   stream.stop();
//
// States reported to onStatus: connecting, waiting (connected, but no frames
// yet or the feed went quiet), live, reconnecting, error, closed.
//
// Frames arrive faster than they can usefully be drawn, so only the most
// recent message is decoded per animation frame. If the connection drops, it
// is retried until stop() is called, unless the failure is permanent (for
// example, the session has expired).
export class VideoStream {
    constructor({ url, onFrame, onStatus }) {
        this.url = url;
        this.onFrame = onFrame;
        this.onStatus = onStatus || (() => {});
        this.pending = null;
        this.raf = 0;
        this.watchdog = 0;
        this.retry = 0;
        this.abort = null;
        this.lastFrameAt = 0;
        this.state = null;
        this.running = false;
    }

    setState(state, detail) {
        if (state === this.state && !detail) {
            return;
        }
        this.state = state;
        this.onStatus(state, detail);
    }

    start() {
        if (this.running) {
            return;
        }
        this.running = true;
        this.lastFrameAt = 0;
        this.watchdog = setInterval(() => {
            if (this.state === "live" && Date.now() - this.lastFrameAt > 2000) {
                this.setState("waiting");
            }
        }, 500);
        this.setState("connecting");
        this.connect();
    }

    async connect() {
        const abort = new AbortController();
        this.abort = abort;
        let permanent = null;
        try {
            const resp = await fetch(this.url, {
                signal: abort.signal,
                cache: "no-store",
                credentials: "same-origin",
            });
            const type = resp.headers.get("content-type") || "";
            if (resp.status === 401 || resp.status === 403
                    || (resp.ok && !type.startsWith("application/octet-stream"))) {
                // A signed-out session gets redirected to the login page
                permanent = "not signed in";
                throw new Error(permanent);
            }
            if (!resp.ok) {
                throw new Error(`HTTP ${resp.status}`);
            }
            this.setState("waiting");

            const reader = new MessageReader();
            const body = resp.body.getReader();
            for (;;) {
                const { done, value } = await body.read();
                if (done) {
                    break;
                }
                for (const msg of reader.push(value)) {
                    this.pending = msg;
                    if (!this.raf) {
                        this.raf = requestAnimationFrame(() => this.draw());
                    }
                }
            }
        } catch (e) {
            if (!this.running || abort.signal.aborted) {
                return;
            }
            if (permanent) {
                this.running = false;
                clearInterval(this.watchdog);
                this.setState("error", permanent);
                return;
            }
        }
        if (!this.running || abort.signal.aborted) {
            return;
        }
        this.setState("reconnecting");
        this.retry = setTimeout(() => {
            this.retry = 0;
            if (this.running) {
                this.connect();
            }
        }, 1000);
    }

    draw() {
        this.raf = 0;
        const data = this.pending;
        this.pending = null;
        if (!data) {
            return;
        }
        try {
            this.onFrame(decodeFrame(data));
            this.lastFrameAt = Date.now();
            this.setState("live");
        } catch (e) {
            this.setState("error", e && e.message ? e.message : String(e));
        }
    }

    stop() {
        this.running = false;
        clearInterval(this.watchdog);
        this.watchdog = 0;
        clearTimeout(this.retry);
        this.retry = 0;
        if (this.raf) {
            cancelAnimationFrame(this.raf);
            this.raf = 0;
        }
        this.pending = null;
        if (this.abort) {
            this.abort.abort();
            this.abort = null;
        }
        this.setState("closed");
    }
}
