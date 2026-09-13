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

use crate::args::{ Args, LedArgs };
use crate::frame::{ Frame, FrameHeader, FrameAttr, PixelFormat };
use crate::viewrect::ViewRect;

use anyhow::{ Result, anyhow };
use rpi_led_matrix::{ LedMatrix, LedMatrixOptions, LedRuntimeOptions, LedCanvas, LedColor };
use tracing::{ debug, info, warn };

type RowRenderFn = fn(
    canvas: &mut LedCanvas,
    row: &[u8],
    src_x: usize,
    dst_x: i32,
    dst_y: i32,
    count: usize,
    x_dir: i32,
);

pub struct Screen {
    matrix: LedMatrix,
    width: i16,
    height: i16,
    canvas: Option<LedCanvas>,
    source_rect: ViewRect,
    dest_rect: ViewRect,
    content_rect: ViewRect,
    blit_src: ViewRect,
    blit_dest: ViewRect,
    vw_origin: i16,
    cached_header: Option<FrameHeader>,
    row_render_fn: Option<RowRenderFn>,
}

impl Screen {
    pub fn new(args: &Args) -> Result<Self> {
        info!("Initializing LED matrix...");

        // Init matrix
        let (mx_options, rt_options) = Self::parse_led_args(&args.led)?;
        let matrix = LedMatrix::new(mx_options, rt_options)
            .map_err(|e| anyhow!("Error initializing matrix: {e}"))?;

        // Init canvas
        let mut canvas = matrix.offscreen_canvas();
        canvas.set(0, 0, &LedColor { red: 0, green: 0, blue: 0 });

        let (screen_width, screen_height) = canvas.canvas_size();
        info!("Initialized canvas of size {}x{}", screen_width, screen_height);

        let screen = Screen {
            matrix: matrix,
            width: screen_width as i16,
            height: screen_height as i16,
            canvas: Some(canvas),
            source_rect: args.source_rect,
            dest_rect: args.dest_rect,
            content_rect: args.content_rect,
            blit_src: args.source_rect,
            blit_dest: args.dest_rect,
            vw_origin: 0,
            cached_header: None,
            row_render_fn: None,
        };

        Ok(screen)
    }

    pub fn dimensions(&self) -> (i16, i16) {
        (self.width, self.height)
    }

    pub fn clear(&mut self) {
        for _ in 0..2 {
            if let Some(mut canvas) = self.canvas.take() {
                canvas.clear();
                self.canvas = Some(self.matrix.swap(canvas));
            }
        }
    }

    fn parse_led_args(led_args: &LedArgs) -> Result<(Option<LedMatrixOptions>, Option<LedRuntimeOptions>)> {
        let mut mx_options = LedMatrixOptions::new();
        mx_options.set_rows(led_args.rows);
        mx_options.set_cols(led_args.cols);
        mx_options.set_chain_length(led_args.chain_length);
        mx_options.set_parallel(led_args.parallel);
        mx_options.set_pwm_bits(led_args.pwm_bits)
            .map_err(|e| anyhow!("Error setting pwm bits: {e}"))?;
        mx_options.set_hardware_mapping(&led_args.gpio_mapping);
        mx_options.set_hardware_pulsing(!led_args.no_hardware_pulse);
        mx_options.set_pwm_lsb_nanoseconds(led_args.pwm_lsb_nanoseconds);
        mx_options.set_pwm_dither_bits(led_args.pwm_dither_bits);
        mx_options.set_refresh_rate(led_args.show_refresh);
        mx_options.set_brightness(led_args.brightness)
            .map_err(|e| anyhow!("Error setting brightness: {e}"))?;
        mx_options.set_scan_mode(led_args.scan_mode as u32);
        mx_options.set_row_addr_type(led_args.row_addr_type as u32);
        mx_options.set_multiplexing(led_args.multiplexing as u32);
        mx_options.set_led_rgb_sequence(&led_args.rgb_sequence);
        mx_options.set_pixel_mapper_config(&led_args.pixel_mapper);
        mx_options.set_panel_type(&led_args.panel_type);
        mx_options.set_inverse_colors(led_args.inverse_colors);
        mx_options.set_limit_refresh(led_args.limit_refresh);

        let mut rt_options = LedRuntimeOptions::new();
        rt_options.set_gpio_slowdown(led_args.gpio_slowdown);
        rt_options.set_drop_privileges(!led_args.no_priv_drop);
        rt_options.set_daemon(led_args.daemon);

        Ok((Some(mx_options), Some(rt_options)))
    }

    fn inspect_geometry(&mut self, header: &FrameHeader) {
        if self.cached_header.is_some_and(|cached| cached == *header) {
            return;
        }

        debug!("Received frame {}", header);

        self.clear();

        self.blit_src = self.source_rect;
        self.blit_dest = self.dest_rect;

        let header_width = header.width as i16;
        let header_height = header.height as i16;

        // Center content
        let mut x_delta = (self.content_rect.x2 - header_width) / 2;
        if self.blit_src.x2 - x_delta < 0 {
            x_delta -= self.blit_src.x2 - x_delta;
        }
        let mut y_delta = (self.content_rect.y2 - header_height) / 2;
        if self.blit_src.y2 - y_delta < 0 {
            y_delta -= self.blit_src.y2 - y_delta;
        }

        if self.blit_src.x1 == 0 {
            self.blit_dest.x1 += x_delta;
            self.blit_src.x2 -= x_delta;
        } else {
            self.blit_dest.x2 += x_delta;
            self.blit_src.x1 -= x_delta;
        }
        if self.blit_src.y1 == 0 {
            self.blit_dest.y1 += y_delta;
            self.blit_src.y2 -= y_delta;
        } else {
            self.blit_dest.y2 += y_delta;
            self.blit_src.y1 -= y_delta;
        }

        // Clamp src rect to actual frame dimensions
        if self.blit_src.x2 > header_width {
            self.blit_src.x2 = header_width;
        }
        if self.blit_src.y2 > header_height {
            self.blit_src.y2 = header_height;
        }

        // Clamp dst rect to canvas dimensions
        let col_count = self.blit_src.x2 - self.blit_src.x1;
        if self.blit_dest.x1 + col_count > self.width {
            self.blit_src.x2 -= self.blit_dest.x1 + col_count - self.width;
        }
        let row_count = self.blit_src.y2 - self.blit_src.y1;
        if self.blit_dest.y1 + row_count > self.height {
            self.blit_src.y2 -= self.blit_dest.y1 + row_count - self.height;
        }

        if (header.attrs & FrameAttr::ROT180) == FrameAttr::ROT180 {
            self.vw_origin = self.blit_dest.x2;
        } else {
            self.vw_origin = 0;
        }

        // Select format-specific row renderer (no branching in the hot loop)
        self.row_render_fn = match header.pixel_format {
            PixelFormat::RGB565 => Some(row_render_rgb565),
            PixelFormat::RGBA8888 => Some(row_render_rgba8888),
            PixelFormat::RGBA5551 => Some(row_render_rgba5551),
            PixelFormat::ARGB8888 => Some(row_render_argb8888),
            other => {
                warn!("No renderer for format {}", other);
                None
            },
        };

        self.cached_header = Some(*header);
    }

    pub fn render(&mut self, frame: &Frame) {
        let header = &frame.header;
        // FIXME: shite name
        self.inspect_geometry(header);

        let Some(row_render_fn) = self.row_render_fn else {
            return;
        };

        let rot180 = header.attrs & FrameAttr::ROT180 == FrameAttr::ROT180;
        let row_count = (self.blit_src.y2 - self.blit_src.y1) as usize;
        let col_count = (self.blit_src.x2 - self.blit_src.x1) as usize;

        // For rot180: vertical flip is handled by rry; horizontal flip uses x_dir=-1
        // starting from vw_origin - blit_dest.sx so pixel i lands at vw_origin - blit_dest.sx - i
        let dst_x = if rot180 {
            self.vw_origin - self.blit_dest.x1
        } else {
            self.blit_dest.x1
        } as i32;
        let x_dir = if rot180 {
            -1
        } else {
            1
        } as i32;

        let Some(mut canvas) = self.canvas.take() else {
            return;
        };

        for yo in 0..row_count {
            let ry = self.blit_src.y1 as usize + yo;
            let rry = if rot180 {
                header.height as usize - 1 - ry
            } else {
                ry
            };
            let pitch = frame.header.pitch as usize;
            let start = rry * pitch;
            let row = &frame.content[start..start + pitch];

            row_render_fn(
                &mut canvas,
                row,
                self.blit_src.x1 as usize,
                dst_x,
                (self.blit_dest.y1 as usize + yo) as i32,
                col_count,
                x_dir,
            );
        }

        self.canvas = Some(self.matrix.swap(canvas));
    }
}

fn row_render_rgb565(
    canvas: &mut LedCanvas,
    row: &[u8],
    src_x: usize,
    dst_x: i32,
    dst_y: i32,
    count: usize,
    x_dir: i32,
) {
    let start = src_x * 2;
    let pixels = &row[start..start + count * 2];
    for (i, chunk) in pixels.chunks_exact(2).enumerate() {
        let c = u16::from_le_bytes([chunk[0], chunk[1]]);
        canvas.set(
            dst_x + i as i32 * x_dir,
            dst_y,
            &LedColor {
                red: (((c >> 11) & 0x1f) * 255 / 31) as u8,
                green: (((c >> 5) & 0x3f) * 255 / 63) as u8,
                blue: ((c & 0x1f) * 255 / 31) as u8,
            },
        )
    }
}

fn row_render_argb8888(
    canvas: &mut LedCanvas,
    row: &[u8],
    src_x: usize,
    dst_x: i32,
    dst_y: i32,
    count: usize,
    x_dir: i32,
) {
    let start = src_x * 4;
    let pixels = &row[start..start + count * 4];
    for (i, chunk) in pixels.chunks_exact(4).enumerate() {
        let c = u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]);
        canvas.set(
            dst_x + i as i32 * x_dir,
            dst_y,
            &LedColor {
                red: ((c >> 16) & 0xff) as u8,
                green: ((c >> 8) & 0xff) as u8,
                blue: (c & 0xff) as u8,
            }
        )
    }
}

fn row_render_rgba8888(
    canvas: &mut LedCanvas,
    row: &[u8],
    src_x: usize,
    dst_x: i32,
    dst_y: i32,
    count: usize,
    x_dir: i32,
) {
    let start = src_x * 4;
    let pixels = &row[start..start + count * 4];
    for (i, chunk) in pixels.chunks_exact(4).enumerate() {
        let c = u32::from_le_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]);
        canvas.set(
            dst_x + i as i32 * x_dir,
            dst_y,
            &LedColor {
                red: ((c >> 24) & 0xff) as u8,
                green: ((c >> 16) & 0xff) as u8,
                blue: ((c >> 8) & 0xff) as u8,
            }
        )
    }
}

fn row_render_rgba5551(
    canvas: &mut LedCanvas,
    row: &[u8],
    src_x: usize,
    dst_x: i32,
    dst_y: i32,
    count: usize,
    x_dir: i32,
) {
    let start = src_x * 2;
    let pixels = &row[start..start + count * 2];
    for (i, chunk) in pixels.chunks_exact(2).enumerate() {
        let c = u16::from_le_bytes([chunk[0], chunk[1]]);
        canvas.set(
            dst_x + i as i32 * x_dir,
            dst_y,
            &LedColor {
                red: (((c >> 11) & 0x1f) * 255 / 31) as u8,
                green: (((c >> 6) & 0x1f) * 255 / 31) as u8,
                blue: (((c >> 1) & 0x1f) * 255 / 31) as u8,
            },
        )
    }
}
