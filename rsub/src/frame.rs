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

pub struct Frame<'a> {
    pub header: FrameHeader,
    pub content: &'a [u8],
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct FrameHeader {
    pub pitch: u16,
    pub width: u16,
    pub height: u16,
    pub pixel_format: PixelFormat,
    pub attrs: FrameAttr,
}

impl FrameHeader {
    pub const SIZE: usize = size_of::<FrameHeader>();

    pub fn parse(data: &[u8]) -> Option<Self> {
        if data.len() < Self::SIZE {
            return None;
        }
        Some(Self {
            pitch: u16::from_le_bytes([data[0], data[1]]),
            width: u16::from_le_bytes([data[2], data[3]]),
            height: u16::from_le_bytes([data[4], data[5]]),
            pixel_format: PixelFormat(data[6]),
            attrs: FrameAttr(data[7]),
        })
    }

    pub fn decompressed_size(&self) -> Option<usize> {
        (self.pitch as usize).checked_mul(self.height as usize)
    }
}

impl fmt::Display for FrameHeader {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}x{} (pitch: {}), {}, {}",
            self.width, self.height, self.pitch, self.pixel_format, self.attrs,
        )
    }
}

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct PixelFormat(pub u8);

impl PixelFormat {
    pub const UNKNOWN: Self = Self(0);
    pub const RGB565: Self = Self(1);
    pub const RGBA8888: Self = Self(2);
    pub const RGBA5551: Self = Self(3);
    pub const ARGB8888: Self = Self(4);
}

impl fmt::Display for PixelFormat {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match *self {
            Self::RGB565 => f.write_str("RGB565"),
            Self::RGBA8888 => f.write_str("RGBA8888"),
            Self::RGBA5551 => f.write_str("RGBA5551"),
            Self::ARGB8888 => f.write_str("ARGB8888"),
            Self::UNKNOWN => f.write_str("UNKNOWN"),
            other => write!(f, "({})", other.0),
        }
    }
}

#[repr(transparent)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub struct FrameAttr(pub u8);

impl FrameAttr {
    pub const ROT180: Self = Self(0x01);

    const FLAGS: &'static [(Self, &'static str)] = &[
        (Self::ROT180, "rotated"),
    ];
}

impl fmt::Display for FrameAttr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.0 == 0 {
            return f.write_str("none");
        }

        let mut remaining = self.0;
        let mut sep = false;
        for &(flag, name) in Self::FLAGS {
            if remaining & flag.0 == flag.0 {
                if sep {
                    f.write_str("|")?;
                }
                f.write_str(name)?;
                remaining &= !flag.0;
                sep = true;
            }
        }
        if remaining != 0 {
            if sep {
                f.write_str("|")?;
            }
            write!(f, "0x{remaining:02x}")?;
        }
        Ok(())
    }
}

impl std::ops::BitAnd for FrameAttr {
    type Output = Self;
    fn bitand(self, rhs: Self) -> Self {
        Self(self.0 & rhs.0)
    }
}

impl std::ops::BitOr for FrameAttr {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self {
        Self(self.0 | rhs.0)
    }
}
