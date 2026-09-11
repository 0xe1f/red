// Copyright (c) 2026 Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

mod args;
mod frame;

use crate::args::{ Args };
use crate::frame::{ FrameHeader, FrameAttr };

use std::fs::OpenOptions;
use std::io;
use std::sync::Mutex;
use std::time::{ Duration, Instant };

use anyhow::{ Context, Result, bail };
use async_nats::{ ConnectOptions };
use futures::StreamExt;
use tokio::{ pin, select, signal, time };
use tracing::{ debug, info, warn, error };
use tracing_subscriber::fmt;

const SUBJECT: &str = "red.frames";
const RECONNECT_WAIT: Duration = Duration::from_millis(250);
const FPS_INTERVAL: Duration = Duration::from_secs(3);
const SCREEN_CLEAR_INTERVAL: Duration = Duration::from_secs(1);
const MAX_RECONNECTS: usize = 50;

fn init_logging(args: &Args) -> Result<()> {
    let builder = fmt()
        .with_max_level(args.log_level.tracing_level())
        .with_target(true);

    if let Some(path) = &args.log_path {
        let file = OpenOptions::new()
            .create(true)
            .write(true)
            .truncate(args.log_overwrite)
            .append(!args.log_overwrite)
            .open(path)
            .with_context(|| format!("Failed to open log file '{}'", path.display()))?;
        builder.with_writer(Mutex::new(file)).init();
        info!(
            "{} output to '{}'",
            if args.log_overwrite {
                "Writing"
            } else {
                "Appending"
            },
            path.display()
        )
    } else {
        builder.with_writer(io::stderr).init();
    }

    Ok(())
}

fn matrix_init() -> Result<(i16, i16)> {
    // FIXME
    Ok((320, 240))
}

fn matrix_clear() {
    // FIXME
}

fn extract_frame(payload: &[u8], decomp_buf: &mut Vec<u8>, last_header: &mut Option<FrameHeader>) -> Result<()> {
    // Parse frame header into struct
    let header = FrameHeader::parse(payload).context("Message too short")?;
    let Some(decomp_size) = header.decompressed_size() else {
        bail!("Invalid frame geometry: {header}");
    };
    if last_header.as_ref().is_none_or(|prev| !prev.same_geometry(&header)) {
        debug!(%header, "Received frame with geometry");
        *last_header = Some(header);
    }

    // Sanity checks
    if decomp_size == 0 {
        bail!("Empty frame: {header}");
    }
    if decomp_buf.len() < decomp_size {
        decomp_buf.resize(decomp_size, 0);
    }

    // Decompress
    let compressed = &payload[FrameHeader::SIZE..];
    lz4_flex::decompress_into(compressed, &mut decomp_buf[..decomp_size])
        .context("LZ4 decompression failed")?;

    Ok(())
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();
    init_logging(&args)?;

    if args.background {
        // FIXME: daemonize (fork + setsid, close stdio) like the C subscriber's --background.
        warn!("--background is not implemented; staying in the foreground");
    }

    // Initialize matrix
    let (screen_width, screen_height) = matrix_init()?;

    // Arg check
    if let Some(rect) = args.src_rect {
        debug!("Source: {rect}");
    }
    if let Some(rect) = args.dest_rect {
        debug!("Destination: {rect}");
        if rect.x2 > screen_width {
            bail!("Destination x2 ({}) exceeds max ({})", rect.x2, screen_width)
        } else if rect.y2 > screen_height {
            bail!("Destination y2 ({}) exceeds max ({})", rect.y2, screen_height)
        }
    }
    if let Some(rect) = args.content_rect {
        debug!("Content: {rect}");
    }

    // Init NATS
    let client = ConnectOptions::new()
        .retry_on_initial_connect()
        .max_reconnects(Some(MAX_RECONNECTS))
        .reconnect_delay_callback(|_| RECONNECT_WAIT)
        .connect(&args.server_url)
        .await
        .with_context(|| format!("connecting to NATS at '{}'", args.server_url))?;
    info!("Connected to NATS server at {}", args.server_url);

    let mut subscriber = client
        .subscribe(SUBJECT)
        .await
        .with_context(|| format!("subscribing to '{SUBJECT}'"))?;
    info!("Subscribed to '{SUBJECT}'");

    let mut decomp_buf = Vec::new();
    let mut last_header: Option<FrameHeader> = None;
    let mut frames_total = 0u64;
    let mut frames_window = 0u64;
    let mut window_started = Instant::now();
    let mut last_frame_time: Option<Instant> = None;

    let ctrl_c = signal::ctrl_c();
    pin!(ctrl_c);

    loop {
        select! {
            _ = &mut ctrl_c => {
                info!("Caught SIGINT, exiting...");
                break;
            }
            msg = subscriber.next() => {
                let Some(msg) = msg else {
                    warn!("NATS subscription closed");
                    break;
                };
                if let Err(err) = extract_frame(&msg.payload, &mut decomp_buf, &mut last_header) {
                    error!("{err:#}");
                    continue;
                }
                frames_total += 1;
                frames_window += 1;
                let now = Instant::now();
                last_frame_time = Some(now);
                let elapsed = window_started.elapsed();
                if elapsed >= FPS_INTERVAL {
                    let fps = frames_window as f64 / elapsed.as_secs_f64();
                    info!("fps: {fps:.2} ({frames_window} frames, {frames_total} total)");
                    frames_window = 0;
                    window_started = now;
                }
                // TODO: blit onto matrix
            }
            _ = time::sleep(SCREEN_CLEAR_INTERVAL), if last_frame_time.is_some() => {
                // No frames for a while; clear screen
                info!("No frames received for {SCREEN_CLEAR_INTERVAL:?}, clearing canvas...");
                matrix_clear();
                last_frame_time = None;
            }
        }
    }
    info!("Done. Received {frames_total} frames");

    Ok(())
}
