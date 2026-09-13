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

use std::path::PathBuf;

use clap::{ Parser, ValueEnum };

use crate::viewrect::ViewRect;

#[derive(Debug, clap::Args)]
pub struct LedArgs {
    /// Panel rows. Typically 8, 16, 32 or 64
    #[arg(long = "led-rows", default_value_t = 32)]
    pub rows: u32,

    /// Panel columns. Typically 32 or 64
    #[arg(long = "led-cols", default_value_t = 32)]
    pub cols: u32,

    /// Number of daisy-chained panels
    #[arg(long = "led-chain", default_value_t = 1)]
    pub chain_length: u32,

    /// Number of parallel chains
    #[arg(long = "led-parallel", default_value_t = 1)]
    pub parallel: u32,

    /// Slowdown GPIO. Needed for faster Pis and/or slower panels
    #[arg(long = "led-slowdown-gpio", default_value_t = 1)]
    pub gpio_slowdown: u32,

    /// GPIO mapping (regular, adafruit-hat, adafruit-hat-pwm, compute-module)
    #[arg(long = "led-gpio-mapping", default_value = "regular")]
    pub gpio_mapping: String,

    /// PWM bits
    #[arg(long = "led-pwm-bits", default_value_t = 11)]
    pub pwm_bits: u8,

    /// PWM nanoseconds for LSB
    #[arg(long = "led-pwm-lsb-nanoseconds", default_value_t = 130)]
    pub pwm_lsb_nanoseconds: u32,

    /// Time dithering of lower bits
    #[arg(long = "led-pwm-dither-bits", default_value_t = 0)]
    pub pwm_dither_bits: u32,

    /// Don't use hardware pin-pulse generation
    #[arg(long = "led-no-hardware-pulse")]
    pub no_hardware_pulse: bool,

    /// Show refresh rate
    #[arg(long = "led-show-refresh")]
    pub show_refresh: bool,
}

#[derive(Debug, Parser)]
#[command(
    name = "sub",
    about = "Subscribe to published video frames on NATS",
)]
pub struct Args {
    /// NATS server URL
    #[arg(short = 's', long = "server-url", required = true)]
    pub server_url: String,

    /// Source rectangle in the published bitmap (x1,y1-x2,y2)
    #[arg(long = "src-rect", visible_alias = "sr", value_name = "x1,y1-x2,y2")]
    pub source_rect: ViewRect,

    /// Destination rectangle on this LED panel (x1,y1-x2,y2)
    #[arg(long = "dest-rect", visible_alias = "dr", value_name = "x1,y1-x2,y2")]
    pub dest_rect: ViewRect,

    /// Content rectangle on the entire LED screen (x1,y1-x2,y2)
    #[arg(long = "content-rect", visible_alias = "cr", value_name = "x1,y1-x2,y2")]
    pub content_rect: ViewRect,

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

    #[command(flatten)]
    pub led: LedArgs,
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
        <Self as Parser>::parse_from(std::env::args_os())
    }
}
