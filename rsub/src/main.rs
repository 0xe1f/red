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

use std::fs::OpenOptions;
use std::io;
use std::sync::Mutex;

use anyhow::{Context, Result};
use tracing::{ debug, info, warn };
use tracing_subscriber::fmt;

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

fn main() -> Result<()> {
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

    Ok(())
}
