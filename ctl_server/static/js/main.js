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
    var syncTimeoutId = -1;
    var lastSync = 0;
    var lastSensorOrientation = null;

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
        $('.game.selected').removeClass('selected');
        $('#status .title').toggleClass('valid', !!game);
        if (!game) {
            $('#stop').hide();
            $('#status .title')
                .text('No Game Selected');
        } else {
            $('#stop').show();
            $('#status .title')
                .text(game.title);
            $(`.game[data-id="${game.id}"]`)
                .addClass('selected');
        }
    };
    const syncSidebar = function() {
        // Synchronize sidebar filter selection with data from URL hash
        const map = hashMap();
        $('.filter')
            .removeClass('selected');
        if (map.filters) {
            map.filters
                .split(',')
                .forEach((f) => {
                    const $filter = $(`.filter[data-id~="${f}"`);
                    $filter
                        .addClass('selected');
                    $filter
                        .closest('.filters')
                        .addClass('active');
                })
        }
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
        const searchTerm = $("#search:focus").val() || "";
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
    var syncState = function() {
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
                        const map = hashMap(destructureFilters = true);
                        const filterOrientation = (map.filters || {}).o;
                        const sensorOrientation = response.orientation.toLowerCase();

                        if (
                            filterOrientation != sensorOrientation &&
                            sensorOrientation != lastSensorOrientation &&
                            orientations.includes(sensorOrientation)
                        ) {
                            // Auto-orient based on sensor reading
                            $(`.filter.orientation`)
                                .removeClass('selected');
                            $(`.filter.orientation.${sensorOrientation}`)
                                .addClass('selected');
                            lastSensorOrientation = sensorOrientation;
                            syncGameList();
                        }
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
        // Set up filter click handlers
        $('.filter')
            .on("click", function(e) {
                const $item = $(`#filters .filter[data-id="${$(this).data("id")}"]`);
                const $filters = $item
                    .closest('.filters');
                const bewl = !$filters.hasClass('multi') || !e.shiftKey;
                if (bewl && !$item.hasClass('selected')) {
                    $filters
                        .find('.filter')
                        .removeClass('selected');
                }
                $item.toggleClass('selected');
                $filters
                    .toggleClass('active', $filters.find('.selected').length > 0);
                syncGameList();
            });
        syncSidebar();
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
            }
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
                toggleScrim(false);
                updateSelection(resp.title);
            }
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
                toggleScrim(false);
                updateSelection();
            }
        });
    };
    const uiVolume = function() {
        return parseInt($('#volume-slider').val(), 10) || 0;
    };
    const setUiVolume = function(value) {
        $('#volume-slider')
            .val(value)
            .trigger('change');
    };
    const fetchFilters = function() {
        $.ajax({
            url: "filters",
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(response) {
                const $filtersContainer = $("#filters");
                $filtersContainer.empty();
                $.each(response, function(_, entry) {
                    $filtersContainer
                        .append(
                            $("<div />", { "id": entry.id, "class": `filters ${entry.type}` })
                                .append(
                                    $("<span />", { "class": "label", text: entry.label })
                                )
                                .append(
                                    $("<div />", { "class": "scroller" })
                                        .append(
                                            $.map(
                                                entry.options,
                                                function(option) {
                                                    return $("<span />", {
                                                        "class": `${entry.id} filter ${option.name}`,
                                                        "data-id": `${entry.prefix}:${option.name}`
                                                    })
                                                        .append(
                                                            $("<span />", { "class": "title" })
                                                                .text(option.name)
                                                        )
                                                        .append(
                                                            $("<span />", { "class": "count" })
                                                                .text(`(${option.count})`)
                                                        );
                                                }
                                            )
                                        )
                                )
                        );
                });
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
                $.each(response, function(_, entry) {
                    $gamesContainer
                        .append(
                            $("<div />", {
                                "class": `game ${entry.orientation}`,
                                "data-id": entry.id,
                            })
                                .append(
                                    $("<span />", { "class": "title", text: entry.title })
                                )
                                .append(
                                    $("<span />", { "class": "platform", text: entry.app_id })
                                )
                                .append(
                                    $("<span />", { "class": "year", text: entry.year })
                                )
                                .append(
                                    $("<span />", { "class": "company", text: entry.company })
                                )
                                .append(
                                    $("<span />", { "class": "system", text: entry.system })
                                )
                        );
                });
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
                const vol = parseInt($('#volume-slider').val(), 10) || 0;
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
        // prevent shift-select - interferes with tag multiselect
        document.onselectstart = function() {
            return false;
        }
        // set up search handler
        $("#search").on("input", function(e) {
            syncGameList();
        });
        // set up keyboard handler
        $(document).on("keyup", onKeyPressed);
        // set up drag-and-drop upload handlers
        $('#content')
            .on('dragenter', function(e) {
                $('#drop_target').addClass('hovering');
                e.preventDefault();
                e.stopPropagation();
            });
        $('#drop_target')
            .on('dragover', function(e) {
                e.preventDefault();
                e.stopPropagation();
            })
            .on('dragleave', function(e) {
                $(this).removeClass('hovering');
                e.preventDefault();
                e.stopPropagation();
            })
            .on('drop', function(e) {
                $(this).removeClass('hovering');
                if (e.originalEvent.dataTransfer) {
                    upload(e.originalEvent.dataTransfer.files);
                }
                e.preventDefault();
                e.stopPropagation();
        });
        // set up stop button handler
        $("#stop").on("click", function() {
            toggleScrim(true);
            stop();
        });
        $(window).on('hashchange', function() {
            syncSidebar();
            syncGameList();
        });
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
                toggleScrim(false);
            }
        });
    };
    const initMenus = function() {
        $("body")
            .append($("<ul />", { "id": "menu-user-options", "class": "menu" })
                .append($("<li />", { "class": "menu-volume" })
                    .append($("<input />", { "type": "range", "id": "volume-slider" }))
                )
                .append($("<li />", { "class": "divider" }))
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
    initialize();
    syncState();
});
