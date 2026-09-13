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
mod screen;
mod viewrect;

use crate::args::{ Args };
use crate::frame::{ FrameDecoder };
use crate::screen::{ Screen };

use std::fs::OpenOptions;
use std::io;
use std::sync::Mutex;
use std::time::{ Duration, Instant };

use anyhow::{ Context, Result, bail };
use async_nats::{ ConnectOptions };
use futures::StreamExt;
use tokio::{ pin, select, signal, time };
use tracing::{ debug, info, warn, error };
use tracing_subscriber::{ fmt, EnvFilter };

const SUBJECT: &str = "red.frames";
const RECONNECT_WAIT: Duration = Duration::from_millis(250);
const FPS_INTERVAL: Duration = Duration::from_secs(1);
const SCREEN_CLEAR_INTERVAL: Duration = Duration::from_secs(1);
const MAX_RECONNECTS: usize = 50;

fn init_logging(args: &Args) -> Result<()> {
    let log_level = args.log_level.tracing_level();
    let filter = EnvFilter::try_from_default_env()
        .unwrap_or_else(|_| EnvFilter::new(format!("warn,rsub={log_level}")));
    let builder = fmt()
        .with_env_filter(filter)
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

fn main() -> Result<()> {
    run(Args::parse())
}

#[tokio::main]
async fn run(args: Args) -> Result<()> {
    init_logging(&args)?;

    // Initialize matrix
    let mut screen = Screen::new(&args)?;
    let (screen_width, screen_height) = screen.dimensions();

    // Rects
    debug!("Source: {}", args.source_rect);
    debug!("Destination: {}", args.dest_rect);
    if args.dest_rect.x2 > screen_width {
        bail!("Destination x2 ({}) exceeds max ({})", args.dest_rect.x2, screen_width)
    } else if args.dest_rect.y2 > screen_height {
        bail!("Destination y2 ({}) exceeds max ({})", args.dest_rect.y2, screen_height)
    }
    debug!("Content: {}", args.content_rect);

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
    debug!("Subscribed to '{SUBJECT}'");

    let mut frames_total = 0u64;
    let mut frames_window = 0u64;
    let mut window_started = Instant::now();
    let mut last_frame_time: Option<Instant> = None;
    let mut decoder = FrameDecoder::new();

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
                match decoder.decode(&msg.payload) {
                    Ok(frame) => screen.render(&frame),
                    Err(err) => {
                        error!("{err:#}");
                        continue;
                    },
                };
                frames_total += 1;
                let now = Instant::now();
                last_frame_time = Some(now);

                if args.show_fps {
                    frames_window += 1;
                    let elapsed = window_started.elapsed();
                    if elapsed >= FPS_INTERVAL {
                        let fps = frames_window as f64 / elapsed.as_secs_f64();
                        info!("fps: {fps:.2} ({frames_window} frames, {frames_total} total)");
                        frames_window = 0;
                        window_started = now;
                    }
                }
            }
            _ = time::sleep(SCREEN_CLEAR_INTERVAL), if last_frame_time.is_some() => {
                // No frames for a while; clear screen
                debug!("No frames received for {SCREEN_CLEAR_INTERVAL:?}, clearing canvas...");
                screen.clear();
                last_frame_time = None;
            }
        }
    }
    info!("Done. Received {frames_total} frames");

    Ok(())
}
