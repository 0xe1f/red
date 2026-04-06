/*****************************************************************************
 ** Copyright (C) 2024 Akop Karapetyan
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 ******************************************************************************
**/

const minLength = 8;
const suggDigits = 2;
const suggSymbols = 2;
const suggUpcase = 2;
const suggLocase = 2;

function matchCount(re, str) {
    return ((str || '').match(re) || []).length;
}

function getWeights() {
    const pw = $('input[name="password"]').val();
    const digitCount = matchCount(/[0-9]/g, pw);
    const symCount = matchCount(/[!@#$%^&*()\-_=+[\]{}\\;:'",<.>/?|]/g, pw);
    const upcaseCount = matchCount(/\p{Lu}/gu, pw);
    const locaseCount = matchCount(/\p{Ll}/gu, pw);

    const minWeight = Math.min(pw.length / minLength, 1.0);
    const suggWeight = (
        minWeight
        + Math.min(digitCount / suggDigits, 1.0)
        + Math.min(symCount / suggSymbols, 1.0)
        + Math.min(upcaseCount / suggUpcase, 1.0)
        + Math.min(locaseCount / suggLocase, 1.0)
    ) / 5.0;

    return [minWeight, suggWeight];
}

function updatePwStrengthIndicator() {
    const [min, sugg] = getWeights();
    console.debug(`${min} - ${sugg} -- ${sugg * 100.0}%`)
    let strength;
    if (min < 1) {
        strength = "bad";
    } else if (sugg >= .75) {
        strength = "good";
    } else {
        strength = "poor";
    }
    $(".password-strength .gauge")
        .css({ width: `${Math.round(sugg * 100.0)}%` })
        .removeClass(["bad", "poor", "good"])
        .addClass(strength);
}

$().ready(function() {
    $('input[name="password"]')
        .on("change", function() {
            updatePwStrengthIndicator();
        })
        .on("keyup", function() {
            updatePwStrengthIndicator();
        });
    updatePwStrengthIndicator();
});
