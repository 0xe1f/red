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
