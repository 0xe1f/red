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

use std::fmt;
use std::str::FromStr;
use std::path::PathBuf;

use clap::{ Parser, ValueEnum };

#[derive(Debug, Parser)]
#[command(
    name = "sub",
    about = "Subscribe to published video frames on NATS",
    after_help = "LED-matrix flags (--led-*) are accepted and ignored."
)]
pub struct Args {
    /// NATS server URL
    #[arg(short = 's', long = "server-url", required = true)]
    pub server_url: String,

    /// Source rectangle in the published bitmap (x1,y1-x2,y2)
    #[arg(long = "src-rect", visible_alias = "sr", value_name = "x1,y1-x2,y2", required = true)]
    pub src_rect: Option<ViewRect>,

    /// Destination rectangle on this LED panel (x1,y1-x2,y2)
    #[arg(long = "dest-rect", visible_alias = "dr", value_name = "x1,y1-x2,y2", required = true)]
    pub dest_rect: Option<ViewRect>,

    /// Content rectangle on the entire LED screen (x1,y1-x2,y2)
    #[arg(long = "content-rect", visible_alias = "cr", value_name = "x1,y1-x2,y2", required = true)]
    pub content_rect: Option<ViewRect>,

    /// Run in background as daemon
    #[arg(long = "background", visible_alias = "bg")]
    pub background: bool,

    /// Log level
    #[arg(short = 'l', long = "log-level", value_enum, default_value_t = LogLevel::Info)]
    pub log_level: LogLevel,

    /// Log file path
    #[arg(short = 'o', long = "output")]
    pub log_path: Option<PathBuf>,

    /// Overwrite log file (instead of append)
    #[arg(long = "output-overwrite", visible_alias = "oo")]
    pub log_overwrite: bool,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct ViewRect {
    pub x1: i16,
    pub y1: i16,
    pub x2: i16,
    pub y2: i16,
}

impl ViewRect {
    fn validate(&self) -> Result<(), String> {
        if self.x1 >= self.x2 {
            Err(format!("'x1' ({}) equals or exceeds 'x2' ({})", self.x1, self.x2))
        } else if self.y1 >= self.y2 {
            Err(format!("'y1' ({}) equals or exceeds 'y2' ({})", self.y1, self.y2))
        } else {
            Ok(())
        }
    }
}

impl FromStr for ViewRect {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let (start, end) = s
            .split_once('-')
            .ok_or_else(|| format!("'{s}' is not a valid rectangle"))?;
        let (x1, y1) = start
            .split_once(',')
            .ok_or_else(|| format!("'{s}' is not a valid rectangle"))?;
        let (x2, y2) = end
            .split_once(',')
            .ok_or_else(|| format!("'{s}' is not a valid rectangle"))?;
        let rect = Self {
            x1: x1.parse().map_err(|_| format!("'{x1}' is not a valid coordinate"))?,
            y1: y1.parse().map_err(|_| format!("'{y1}' is not a valid coordinate"))?,
            x2: x2.parse().map_err(|_| format!("'{x2}' is not a valid coordinate"))?,
            y2: y2.parse().map_err(|_| format!("'{y2}' is not a valid coordinate"))?,
        };
        rect.validate()?;
        Ok(rect)
    }
}

impl fmt::Display for ViewRect {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "({},{})-({},{})", self.x1, self.y1, self.x2, self.y2)
    }
}

#[derive(Debug, Clone, Copy, Default, ValueEnum)]
pub enum LogLevel {
    #[value(alias = "f")]
    Fatal,
    #[value(alias = "e")]
    Error,
    #[value(alias = "w")]
    Warn,
    #[default]
    #[value(alias = "i")]
    Info,
    #[value(alias = "d")]
    Debug,
    #[value(alias = "v")]
    Verbose,
}

impl LogLevel {
    pub fn tracing_level(self) -> tracing::Level {
        match self {
            Self::Fatal | Self::Error => tracing::Level::ERROR,
            Self::Warn => tracing::Level::WARN,
            Self::Info => tracing::Level::INFO,
            Self::Debug => tracing::Level::DEBUG,
            Self::Verbose => tracing::Level::TRACE,
        }
    }
}

impl Args {
    pub fn parse() -> Self {
        let filtered: Vec<std::ffi::OsString> = std::env::args_os()
            .filter(|arg| {
                arg.to_str()
                    .map(|s| !s.starts_with("--led-"))
                    .unwrap_or(true)
            })
            .collect();
        <Self as Parser>::parse_from(filtered)
    }
}
