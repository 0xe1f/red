$(function() {
    const hashMap = function() {
        return location.hash
            .replace(/^#+/, '')
            .split('&')
            .map((arg) => {
                const pair = arg.split('=', 2);
                return { k: pair[0], v: pair[1] };
            })
            .reduce(function(map, obj) {
                map[obj.k] = obj.v;
                return map;
            }, {});
    };
    const updateSelection = function(id) {
        $('.game.selected').removeClass('selected');
        $('#status .title').toggleClass('valid', !!id);
        if (!id) {
            $('#stop').hide();
            $('#status .title').text('No Game Selected');
            return;
        }
        const $item = $(`.game[data-id="${id}"]`);
        $('#stop').show();
        if ($item.length) {
            $item.addClass('selected');
            $('#status .title').text(
                $item.children('.title').text()
            );
        }
    };
    const syncGames = function() {
        const searchTerm = $("#search:focus").val() || "";
        const filters = $('.filter.selected')
            .map(function() { return $(this).data('id'); })
            .toArray();
        fetchGames(searchTerm, filters);
        location.hash = `filters=${filters.join(',')}`;
    };
    const syncSearch = function(searchTerm, $match) {
        if (!searchTerm) {
            $match.show();
        } else {
            const termLocase = searchTerm.toLowerCase();
            $match.each(
                function() {
                    const title = $(this)
                        .find(".title")
                        .text()
                        .toLowerCase();
                    if (title.startsWith(termLocase)) {
                        $(this).show()
                    }
                }
            );
        }
    };
    const select = function(which) {
        const $selected = $(".active:visible");
        $(".active").removeClass("active");
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
    const closeSearch = function() {
        $("#search").trigger("blur");
        $("#search").val("");
        select(0);
        syncGames();
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
                syncGames();
            });
        // Restore filter from hash
        const map = hashMap();
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
            syncGames();
        }
    };
    const initGames = function() {
        $(".game").on("click", function() {
            const item = $(this);
            const itemId = item.data('id');
            toggleScrim(true);
            launch(itemId);
        });
    };
    const setVolume = function(vol) {
        $.ajax({
            url: "volume",
            type: "POST",
            data: JSON.stringify({ 'volume': vol }),
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(){
                // TODO
            }
        });
    };
    const launch = function(id) {
        $.ajax({
            url: "launch",
            type: "POST",
            data: JSON.stringify({ 'id': id }),
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(){
                toggleScrim(false);
                updateSelection(id);
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
    const updateVolume = function(value) {
        $('#volume')
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
                                                    const countText = `${option.count}`;
                                                    const matchText = (option.count > 99)
                                                        ? `++`
                                                        : countText;

                                                    return $("<span />", {
                                                        "class": `${entry.id} filter`,
                                                        "data-id": `${entry.prefix}:${option.name}`
                                                    })
                                                        .text(option.name)
                                                        .append(
                                                            $("<span />", { "class": "count" })
                                                                .text(matchText)
                                                                .attr("title", countText)
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
                $gamesContainer.empty();
                $.each(response, function(_, entry) {
                    $gamesContainer
                        .append(
                            $("<div />", {
                                "class": "game",
                                "data-id": entry.id,
                                "data-filters": entry.filters.join(' '),
                            })
                                .append(
                                    $("<span />", { "class": "title", text: entry.title })
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
    const initialize = function() {
        // init volume control
        var volumeReady = false;
        $('#volume')
            .knob({
                'fgColor': '#66CC66',
                'angleOffset': -125,
                'angleArc': 250,
                'fgColor': '#66CC66',
                'width': 50,
                'height': 50,
                'change': function(_) { volumeReady = true; },
                'release' : function (v) {
                    if (volumeReady) { setVolume(~~v); }
                },
            });
        updateVolume(0);
        // set current selection
        $.get(
            "query",
            function(resp) {
                updateSelection(resp.title);
                updateVolume(resp.volume || 0);
            },
        );
        updateSelection();
        fetchFilters();
        // prevent shift-select - interferes with tag multiselect
        document.onselectstart = function() {
            return false;
        }
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
    const toggleScrim = function(show) {
        $('body').toggleClass('scrimmed', show);
    };
    initialize();
    $("#stop").on("click", function() {
        toggleScrim(true);
        stop();
    });
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
    $(document).on("keyup", function(e) {
        const $search = $("#search");
        if ($search.is(":focus")) {
            if (e.keyCode == 27) {
                // esc
                closeSearch();
            } else if (e.keyCode == 38) {
                // up
                select(-1);
            } else if (e.keyCode == 40) {
                // down
                select(1);
            } else if (e.keyCode == 13) {
                // return
                const $active = $(".game.active");
                if ($active.length) {
                    launch($active.data("id"));
                    closeSearch();
                    $active[0].scrollIntoView({ behavior: "smooth", block: "center" });
                }
            }
            return;
        }
        if (e.keyCode == 84 || e.keyCode == 116) {
            $search.focus();
        }
    });
    $("#search").on("input", function(e) {
        syncGames();
    });
});
