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

use std::fs::OpenOptions;
use std::io;
use std::sync::Mutex;
use std::time::{ Duration };

use anyhow::{ Context, Result };
use async_nats::{ ConnectOptions };
use futures::StreamExt;
use tokio::{ select, signal };
use tracing::{ debug, info, warn };
use tracing_subscriber::fmt;

const SUBJECT: &str = "red.frames";
const RECONNECT_WAIT: Duration = Duration::from_millis(250);
const MAX_RECONNECTS: usize = 50;

fn init_logging(args: &args::Args) -> Result<()> {
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

#[tokio::main]
async fn main() -> Result<()> {
    let args = args::Args::parse();
    init_logging(&args)?;

    if args.background {
        // FIXME: daemonize (fork + setsid, close stdio) like the C subscriber's --background.
        warn!("--background is not implemented; staying in the foreground");
    }

    // FIXME:
    debug!(
        source = ?args.src_rect,
        dest = ?args.dest_rect,
        content = ?args.content_rect,
        "display rects unused until render is implemented"
    );

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

    let mut frames_total = 0u64;

    loop {
        select! {
            _ = signal::ctrl_c() => {
                info!("Caught SIGINT, exiting...");
                break;
            }
            msg = subscriber.next() => {
                let Some(msg) = msg else {
                    warn!("NATS subscription closed");
                    break;
                };
                // FIXME...
            }
        }
    }
    info!("Done. Received {frames_total} frames");

    Ok(())
}
