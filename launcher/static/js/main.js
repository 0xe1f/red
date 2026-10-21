/*****************************************************************************
 ** Copyright (C) 2024-2025 Akop Karapetyan
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

$(function() {
    const cookies = Cookies.withAttributes({ expires: 31 });
    const orientations = [ 'portrait', 'landscape' ];
    const familyIds = [ 'system', 'genre', 'tag', 'series' ];
    var syncTimeoutId = -1;
    var lastSync = 0;
    var lastSensorOrientation = null;
    var openFamily = null;
    var currentGame = null;

    const hashMap = function(destructureFilters = false) {
        return location.hash
            .replace(/^#+/, '')
            .split('&')
            .map((arg) => {
                var [key, value] = arg.split('=', 2);
                if (destructureFilters && key == 'filters') {
                    value = value
                        .split(',')
                        .map((arg) => {
                            const pair = arg.split(':', 2);
                            return { k: pair[0], v: pair[1] };
                        })
                        .reduce(function(map, obj) {
                            if (!map[obj.k]) {
                                map[obj.k] = [];
                            }
                            map[obj.k].push(obj.v);
                            return map;
                        }, {});
                }
                return { k: key, v: value };
            })
            .reduce(function(map, obj) {
                map[obj.k] = obj.v;
                return map;
            }, {});
    };
    const buildHash = function(map) {
        if (!map) {
            return null;
        }
        return Object.keys(map)
            .filter(function(key) {
                return !!key;
            })
            .map(function(key) {
                var value = map[key];
                if (key == 'filters' && typeof value !== 'string') {
                    value = Object.keys(value)
                        .filter(function(subkey) {
                            return !!subkey;
                        })
                        .map(function(subkey) {
                            return value[subkey]
                                .map(function(v) {
                                    return `${subkey}:${v}`;
                                })
                                .join(',');
                        })
                        .join(',');
                }
                return `${key}=${value}`;
            })
            .join('&');
    };
    const updateSelection = function(game) {
        currentGame = game || null;
        $('.game.selected').removeClass('selected');
        $('.replay button').removeClass('on');
        $('body').toggleClass('running', !!currentGame);
        if (!currentGame) {
            $('#session-kicker').text('Idle');
            $('#session-title')
                .text('No game running')
                .removeAttr('title');
            $('#session-meta')
                .text('Choose a title. The sensor can still set orientation.');
            $('#session-bar-title')
                .text('No game')
                .attr('title', 'No game');
        } else {
            const meta = [ currentGame.system, currentGame.year, currentGame.company, currentGame.orientation ]
                .filter(Boolean)
                .join(' · ');
            $('#session-kicker').text('Running');
            $('#session-title')
                .text(currentGame.title)
                .attr('title', currentGame.title);
            $('#session-meta').text(meta);
            $('#session-bar-title')
                .text(currentGame.title)
                .attr('title', currentGame.title);
            $(`.game[data-id="${currentGame.id}"]`)
                .addClass('selected');
        }
    };
    const showFamily = function(id) {
        openFamily = id || null;
        $('#filters > .filters').removeClass('is-open');
        $('.fam').removeClass('on').attr('aria-pressed', 'false');
        if (openFamily) {
            $(`#${openFamily}`).addClass('is-open');
            $(`.fam[data-family="${openFamily}"]`)
                .addClass('on')
                .attr('aria-pressed', 'true');
            $('#filter-hint').hide();
        } else {
            $('#filter-hint').show();
        }
    };
    const clearFamiliesExcept = function(keep) {
        familyIds.forEach(function(id) {
            if (id !== keep) {
                $(`#${id} .filter`).removeClass('selected');
            }
        });
    };
    const updatePills = function() {
        const $box = $('#active-filters').empty();
        $('#orientation .filter.selected').each(function() {
            const name = $(this).find('.title').text();
            $box.append(
                $('<button />', { type: 'button', class: 'pill', text: `${name} ×` })
                    .attr('data-clear', 'orientation')
            );
        });
        const family = familyIds.find(function(id) {
            return $(`#${id} .filter.selected`).length > 0;
        });
        if (family) {
            const names = $(`#${family} .filter.selected`)
                .map(function() { return $(this).find('.title').text(); })
                .get();
            $box.append(
                $('<button />', { type: 'button', class: 'pill', text: `${names.join(' + ')} ×` })
                    .attr('data-clear', 'family')
            );
        }
    };
    const syncSidebar = function(fromHash) {
        // Synchronize filter selection with data from URL hash.
        // Orientation can stay on. System, genre, tag, and series replace one another.
        const map = hashMap();
        $('.filter').removeClass('selected');
        if (map.filters) {
            map.filters
                .split(',')
                .forEach((f) => {
                    $(`.filter[data-id~="${f}"]`).addClass('selected');
                });
        }
        const active = familyIds.filter(function(id) {
            return $(`#${id} .filter.selected`).length > 0;
        });
        if (active.length > 1) {
            const keep = active[active.length - 1];
            clearFamiliesExcept(keep);
        }
        const selected = familyIds.filter(function(id) {
            return $(`#${id} .filter.selected`).length > 0;
        });
        if (selected.length) {
            showFamily(selected[0]);
        } else if (fromHash) {
            showFamily(null);
        }
        updatePills();
    };
    const syncHash = function() {
        // Build and apply hash from currently selected filters
        // const searchTerm = $("#search:focus").val() || "";
        const filters = $('.filter.selected')
            .map(function() { return $(this).data('id'); })
            .toArray();

        const url = new URL(location);
        const newHash = filters.length > 0
            ? `filters=${filters.join(',')}`
            : '';
        if (newHash != url.hash.replace(/^#/, '')) {
            url.hash = newHash;
            history.pushState({}, "", url);
        }
    };
    const syncGameList = function() {
        const searchTerm = $("#search").val() || "";
        updatePills();
        const filters = $('.filter.selected')
            .map(function() { return $(this).data('id'); })
            .toArray();
        fetchGames(searchTerm, filters);
        syncHash();
    };
    const select = function(which) {
        const $selected = $(".game.active:visible");
        $(".game.active").removeClass("active");
        if (which == 0) {
            return;
        }
        const $next = (!$selected.length)
            ? (which > 0 ? $(".game:visible:first") : $(".game:visible:last"))
            : (which < 0 ? $selected.prev() : $selected.next());
        $next.addClass("active");
        if ($next.length) {
            $next[0].scrollIntoView({ behavior: "smooth", block: "center" });
        }
    };
    const syncState = function() {
        (function stateUpdater() {
            const timeoutMs = 5000; // 5 seconds
            const now = new Date().getTime();

            const delta = now - lastSync;
            if (delta < timeoutMs) {
                console.debug(`Ignoring sync request (last sync ${delta / 1000}s ago)`);
                return; // No need to sync yet
            }

            if (syncTimeoutId > -1) {
                clearTimeout(syncTimeoutId);
                syncTimeoutId = -1;
            }

            if (!document.hidden) {
                lastSync = now;
                $.ajax({
                    url: "sync",
                    contentType: "application/json; charset=utf-8",
                    dataType: "json",
                    success: function(response) {
                        const sensorOrientation = response.orientation.toLowerCase();
                        if (!orientations.includes(sensorOrientation)) {
                            return;
                        }
                        // Remember the reading even when the filter already matches.
                        // Otherwise the next poll looks like a new reading and
                        // overwrites a manual choice once.
                        if (sensorOrientation === lastSensorOrientation) {
                            return;
                        }
                        const $match = $(`#orientation .filter.${CSS.escape(sensorOrientation)}`);
                        if (!$match.length) {
                            return;
                        }
                        lastSensorOrientation = sensorOrientation;
                        if ($match.hasClass('selected')) {
                            return;
                        }
                        $('#orientation .filter').removeClass('selected');
                        $match.addClass('selected');
                        syncGameList();
                    }
                })
                .always(function() {
                    syncTimeoutId = setTimeout(stateUpdater, timeoutMs);
                });
            } else {
                syncTimeoutId = setTimeout(stateUpdater, timeoutMs);
            }
        })();
    };
    const closeSearch = function() {
        $("#search").trigger("blur");
        $("#search").val("");
        select(0);
        syncGameList();
    };
    const initFilters = function() {
        // Orientation toggles on its own. Any other family replaces the rest.
        // Tags combine with each other on click.
        $('.filter').on("click", function() {
            const $item = $(this);
            const id = $item.closest('.filters').attr('id');
            if (id === 'orientation') {
                const wasOn = $item.hasClass('selected');
                $('#orientation .filter').removeClass('selected');
                if (!wasOn) {
                    $item.addClass('selected');
                }
            } else if (id === 'tag') {
                if (openFamily !== 'tag') {
                    clearFamiliesExcept('tag');
                    $('#tag .filter').removeClass('selected');
                    $item.addClass('selected');
                    showFamily('tag');
                } else {
                    $item.toggleClass('selected');
                    if (!$('#tag .filter.selected').length) {
                        showFamily(null);
                    }
                }
            } else {
                const wasOn = openFamily === id && $item.hasClass('selected');
                clearFamiliesExcept(id);
                $(`#${id} .filter`).removeClass('selected');
                if (wasOn) {
                    showFamily(null);
                } else {
                    $item.addClass('selected');
                    showFamily(id);
                }
            }
            syncGameList();
        });
        $('.fam').on("click", function() {
            const id = $(this).attr('data-family');
            if (openFamily === id) {
                clearFamiliesExcept(null);
                showFamily(null);
            } else {
                clearFamiliesExcept(id);
                $(`#${id} .filter`).removeClass('selected');
                showFamily(id);
            }
            syncGameList();
        });
        syncSidebar(true);
        syncGameList();
    };
    const initGames = function() {
        $(".game").on("click", function() {
            const item = $(this);
            const itemId = item.data('id');
            launch(itemId);
        });
    };
    const setVolume = function(vol, clearMute = true) {
        $.ajax({
            url: "volume",
            type: "POST",
            data: JSON.stringify({ 'volume': vol }),
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(response) {
                setUiVolume(response.volume);
            },
            error: function() {
                // FIXME: display error message
            },
            complete: function() {
            },
        });
        if (clearMute) {
            cookies.remove('pre_mute_vol');
        }
    };
    const launch = function(id) {
        toggleScrim(true);
        $.ajax({
            url: "launch",
            type: "POST",
            data: JSON.stringify({ 'id': id }),
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(resp){
                updateSelection(resp.title);
            },
            error: function() {
                // FIXME: display error message
            },
            complete: function() {
                toggleScrim(false);
            },
        });
    };
    const stop = function() {
        $.ajax({
            url: "stop",
            type: "POST",
            data: JSON.stringify({}),
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(){
                updateSelection();
            },
            error: function() {
                // FIXME: display error message
            },
            complete: function() {
                toggleScrim(false);
            },
        });
    };
    const uiVolume = function() {
        return parseInt($('#volume-slider').val(), 10) || 0;
    };
    const setUiVolume = function(value) {
        $('#volume-slider')
            .val(value)
            .trigger('change');
        $('#volume-value, #session-bar-vol')
            .text(`${value}%`);
        $('#volume-mute')
            .text(value > 0 ? 'Mute' : 'Unmute');
    };
    const toggleMute = function() {
        const vol = uiVolume();
        if (vol > 0) {
            cookies.set('pre_mute_vol', vol);
            setVolume(0, false);
        } else {
            const preMute = cookies.get('pre_mute_vol');
            if (preMute) {
                const newVol = parseInt(preMute, 10);
                setVolume(newVol);
            }
        }
    };
    const orientationIcon = function(name) {
        const portrait = name === 'portrait';
        const body = portrait
            ? '<rect x="7" y="3" width="10" height="18" fill="none" stroke="currentColor" stroke-width="1.6"/>'
            : '<rect x="3" y="7" width="18" height="11" fill="none" stroke="currentColor" stroke-width="1.6"/>';
        return $(`<svg class="ori-icon" viewBox="0 0 24 24" width="16" height="16" aria-hidden="true">${body}</svg>`);
    };
    const fetchFilters = function() {
        $.ajax({
            url: "filters",
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(response) {
                const $filtersContainer = $("#filters").empty();
                const $top = $("<div />", { "class": "fstrip-top" });
                const $fams = $("<div />", { "class": "frow fams" });
                $.each(response, function(_, entry) {
                    const $scroller = $("<div />", { "class": "scroller" });
                    $.each(entry.options, function(_, option) {
                        const $chip = $("<span />", {
                            "class": `${entry.id} filter`,
                            "data-id": `${entry.prefix}:${option.name}`,
                            role: "button",
                            tabindex: 0,
                        }).addClass(option.name);
                        if (entry.id === 'orientation') {
                            $chip.append(orientationIcon(option.name));
                        }
                        $chip
                            .append($("<span />", { "class": "title", text: option.name }))
                            .append($("<span />", { "class": "count", text: `(${option.count})` }));
                        $scroller.append($chip);
                    });
                    if (entry.id === 'tag') {
                        $scroller.append($("<span />", { "class": "hint", text: "Tags combine" }));
                    }
                    const $group = $("<div />", {
                        "id": entry.id,
                        "class": `filters ${entry.type || ''}`,
                    })
                        .append($("<span />", { "class": "label", text: entry.label }))
                        .append($scroller);
                    if (entry.id === 'orientation') {
                        $top.append($group);
                    } else {
                        $fams.append($("<button />", {
                            type: "button",
                            "class": "fam",
                            text: entry.label,
                            "aria-pressed": "false",
                        }).attr("data-family", entry.id));
                        $filtersContainer.append($group);
                    }
                });
                $top.append($fams);
                $filtersContainer
                    .prepend($top)
                    .append($("<div />", {
                        id: "filter-hint",
                        "class": "hint",
                        text: "Pick a family. Orientation can stay on.",
                    }));
                initFilters();
            }
        });
    };
    const fetchGames = function(searchTerm = "", filters = []) {
        $.ajax({
            url: "games",
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            data: {
                "search": searchTerm,
                "filters": filters.join(','),
            },
            success: function(response) {
                const $gamesContainer = $("#games");
                $gamesContainer
                    .removeClass()
                    .empty();
                if (filters.find(filter => filter.startsWith("o:")) !== undefined) {
                    $gamesContainer.addClass("oriented");
                }
                if (filters.find(filter => filter.startsWith("p:")) !== undefined) {
                    $gamesContainer.addClass("platformed");
                }
                if (filters.find(filter => filter.startsWith("y:")) !== undefined) {
                    $gamesContainer.addClass("has-system");
                }
                const countLabel = response.length === 1 ? 'title' : 'titles';
                $('#list-count').text(`${response.length} ${countLabel}`);
                $.each(response, function(_, entry) {
                    const $line = $("<span />", { "class": "line" });
                    if (!$gamesContainer.hasClass("oriented")) {
                        $line.append(orientationIcon(entry.orientation).addClass("game-ori"));
                    }
                    $line
                        .append($("<span />", {
                            "class": "title",
                            text: entry.title,
                            title: entry.title,
                        }))
                        .append($("<span />", { "class": "system", text: entry.system }));
                    $gamesContainer.append(
                        $("<div />", {
                            "class": `game ${entry.orientation}`,
                            "data-id": entry.id,
                        })
                            .append($line)
                            .append($("<span />", {
                                "class": "meta",
                                text: [ entry.year, entry.company ].filter(Boolean).join(' · '),
                            }))
                    );
                });
                if (currentGame) {
                    $(`.game[data-id="${currentGame.id}"]`).addClass('selected');
                }
                initGames();
            }
        });
    };
    const onKeyPressed = function(e) {
        const $search = $("#search");
        var handled = false;
        if ($search.is(":focus")) {
            if (e.keyCode == 27) {
                // esc
                closeSearch();
                handled = true;
            } else if (e.keyCode == 38) {
                // up
                select(-1);
                handled = true;
            } else if (e.keyCode == 40) {
                // down
                select(1);
                handled = true;
            } else if (e.keyCode == 13) {
                // return
                const $active = $(".game.active");
                if ($active.length) {
                    launch($active.data("id"));
                    closeSearch();
                    $active[0].scrollIntoView({ behavior: "smooth", block: "center" });
                }
                handled = true;
            }
        } else {
            if (e.keyCode == 84 || e.keyCode == 191) {
                // 't' or 'T'
                $search.focus();
                handled = true;
            } else if (e.keyCode == 173) {
                // volume down
                const vol = uiVolume();
                const newVol = e.shiftKey
                    ? Math.max(0, vol - 10)
                    : Math.max(0, vol - 2);
                if (vol != newVol) {
                    setVolume(newVol);
                }
                handled = true;
            } else if (e.keyCode == 61) {
                // volume up
                const vol = parseInt($('#volume-slider').val(), 10) || 0;
                const newVol = e.shiftKey
                    ? Math.min(100, vol + 10)
                    : Math.min(100, vol + 2);
                if (vol != newVol) {
                    setVolume(newVol);
                }
                handled = true;
            } else if (e.keyCode == 77) {
                // mute
                toggleMute();
                handled = true;
            } else if (e.keyCode == 80) {
                // 'p', previous
                select(-1);
                handled = true;
            } else if (e.keyCode == 78) {
                // 'n', next
                select(1);
                handled = true;
            } else if (e.keyCode == 13 || e.keyCode == 79) {
                // return or 'o', launch
                const $active = $(".game.active");
                if ($active.length) {
                    launch($active.data("id"));
                    select(0);
                }
                handled = true;
            } else if (e.keyCode == 27) {
                // esc, clear selection
                select(0);
                handled = true;
            }
        }
        return !handled;
    };
    // Replay shortcuts. Handled on keydown (not keyup) because Ctrl may
    // already be released by the time the letter's keyup fires.
    const replayShortcuts = {
        // Ctrl+C: resume recording
        67: 'replay-resume-record',
        // Ctrl+V: end recording
        86: 'replay-stop',
    };
    const onReplayShortcut = function(e) {
        const id = replayShortcuts[e.keyCode];
        if (!id || !e.ctrlKey || e.shiftKey || e.altKey || e.metaKey) {
            return true;
        }
        // Leave copy/paste alone while typing in a field
        if ($(e.target).is("input, textarea, select, [contenteditable]")) {
            return true;
        }
        if (!e.originalEvent.repeat) {
            const button = document.getElementById(id);
            $(".replay button").removeClass("on");
            onReplayAction(button);
        }
        e.preventDefault();
        return false;
    };
    const initialize = function() {
        // init volume control
        initMenus();
        $('#volume-slider').on('input', function() {
            setVolume(~~(this.value));
        });
        setUiVolume(0);
        // set current selection
        $.get(
            "query",
            function(resp) {
                updateSelection(resp.title);
                setUiVolume(resp.volume || 0);
            },
        );
        updateSelection();
        fetchFilters();
        applyNight($("html").hasClass("night"));
        $("#search").on("input", function() {
            syncGameList();
        });
        $("#theme").on("click", function() {
            const on = !$("html").hasClass("night");
            applyNight(on);
            if (on) {
                cookies.set("night", "1");
            } else {
                cookies.remove("night");
            }
        });
        $("#session-toggle, #session-bar-toggle").on("click", function() {
            const open = !$("body").hasClass("session-open");
            $("body").toggleClass("session-open", open);
            $("#session-toggle").text(open ? "Collapse" : "Expand");
        });
        $(".replay button").on("click", function(e) {
            e.preventDefault();
            $(".replay button").removeClass("on");
            onReplayAction(this);
        });
        $("#volume-mute").on("click", function() {
            toggleMute();
        });
        $("#active-filters").on("click", ".pill", function() {
            if ($(this).attr("data-clear") === "orientation") {
                $("#orientation .filter").removeClass("selected");
            } else {
                clearFamiliesExcept(null);
                showFamily(null);
            }
            syncGameList();
        });
        $(document).on("keyup", onKeyPressed);
        $(document).on("keydown", onReplayShortcut);
        $(".stop-game").on("click", function(e) {
            e.preventDefault();
            toggleScrim(true);
            stop();
        });
        $(window).on('hashchange', function() {
            syncSidebar(true);
            syncGameList();
        });
    };
    const applyNight = function(on) {
        $("html").toggleClass("night", !!on);
        $("#theme")
            .attr("aria-pressed", on ? "true" : "false")
            .attr("aria-label", on ? "Switch to day" : "Switch to night")
            .attr("title", on ? "Day" : "Night");
    };
    const upload = function(files) {
        if (files.length == 0) {
            return;
        }

        toggleScrim(true);

        var form = new FormData();
        for (var i = 0; i < files.length; i++) {
            form.append(`files[${i}]`, files[i]);
        }

        $.ajax({
            url: "upload",
            type: "POST",
            cache: false,
            contentType: false,
            processData: false,
            data: form,
            success: function(){
                // TODO
            },
            error: function() {
                // TODO: display error message
            },
            complete: function() {
                toggleScrim(false);
            },
        });
    };
    const initMenus = function() {
        $("body")
            .append($("<ul />", { "id": "menu-user-options", "class": "menu" })
                .append($("<li />", { "class": "menu-sign-out" }).text("Sign out"))
            )
        ;

        $(".menu li").not(".divider").wrapInner("<span />");

        $$menu.click(function(e) {
            var $item = e.$item;
            if ($item.is(".menu-sign-out")) {
                $("#sign-out")[0].click();
            }
            return false;
        });
    };
    const toggleScrim = function(show) {
        $('body').toggleClass('scrimmed', show);
    };
    const onReplayAction = function(e) {
        const endpoint = (() => {
            switch (e.id) {
                case 'replay-play':
                    return {
                        url: 'playback',
                        args: { 'slot': 0 },
                    };
                case 'replay-stop':
                    return {
                        url: 'stop_replay'
                    };
                case 'replay-record':
                    return {
                        url: 'record',
                        args: { 'slot': 0 },
                    };
                case 'replay-resume-record':
                    return {
                        url: 'resume_record',
                        args: { 'slot': 0 },
                    };
                default: return null;
            }
        })();
        if (!endpoint) {
            console.warn(`Unknown replay action: ${e.id}`);
            return;
        }

        $.ajax({
            url: endpoint.url,
            type: "POST",
            data: JSON.stringify(endpoint.args || {}),
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(response) {
                // TODO
            },
            error: function() {
            }
        });
    };

    initialize();
    syncState();
});
