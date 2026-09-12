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

use crate::args::{ Args, ViewRect };
use crate::frame::{ FrameHeader, FrameAttr };

use anyhow::{ Result, anyhow };
use rpi_led_matrix::{ LedMatrix, LedMatrixOptions, LedRuntimeOptions, LedCanvas, LedColor };
use tracing::{ debug, info };

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
}

impl Screen {
    pub fn new(args: &Args) -> Result<Self> {
        info!("Initializing LED matrix...");

        // Init matrix options
        // FIXME: read from args
        let mut options = LedMatrixOptions::new();
        options.set_rows(64);
        options.set_cols(64);
        options.set_chain_length(5);
        options.set_parallel(2);
        options.set_pwm_bits(6)
            .map_err(|e| anyhow!("Error setting pwm bits: {e}"))?;
        options.set_hardware_mapping("regular");

        let mut rt_options = LedRuntimeOptions::new();
        rt_options.set_gpio_slowdown(4);

        // Init matrix
        let matrix = LedMatrix::new(Some(options), Some(rt_options))
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

    pub fn inspect_geometry(&mut self, header: &FrameHeader) {
        // FIXME: check geo

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

        // FIXME: finish
    }
}

// static void inspect_geometry(const FrameHeader *hdr)
// {
//     if (hdr->width == geometry.width &&
//         hdr->height == geometry.height &&
//         hdr->pixel_format == geometry.pixel_format &&
//         hdr->attrs == geometry.attrs
//     ) {
//         return; // same geometry, no need to recalculate blit rectangles
//     }

// ....

//     // Select format-specific row renderer (no branching in the hot loop)
//     switch ((PixelFormat)hdr->pixel_format) {
//         case PF_RGB565:
//             row_render_fn = render_row_rgb565;
//             break;
//         case PF_ARGB8888:
//             row_render_fn = render_row_argb8888;
//             break;
//         case PF_RGBA8888:
//             row_render_fn = render_row_rgba8888;
//             break;
//         case PF_RGBA5551:
//             row_render_fn = render_row_rgba5551;
//             break;
//         default:
//             log_w(LOG_TAG, "Unsupported pixel format: %d\n", hdr->pixel_format);
//             row_render_fn = NULL;
//             break;
//     }

//     geometry = *hdr;
// }
