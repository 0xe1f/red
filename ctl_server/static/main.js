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
    const syncFiltering = function() {
        const $selectedFilters = $('.filter.selected');
        const searchTerm = $("#search:focus").val() || "";
        $('.game').hide();
        if (!$selectedFilters.length) {
            syncSearch(searchTerm, $(".game"));
            location.hash = '';
            return;
        }
        var appliedSymbols = [];
        const matchMap = new Map();
        $selectedFilters
            .each(function(i, obj) {
                const $obj = $(obj);
                const filter = $obj.data('id');
                appliedSymbols.push(filter);
                $(`.game[data-filters~="${filter}"]`)
                    .toArray()
                    .forEach(element => {
                        matchMap.set(element, (matchMap.get(element) ?? 0) + 1);
                    });
                });
        location.hash = `filters=${appliedSymbols.join(',')}`;
        matchMap
            .forEach((v, k) => {
                if (v == $selectedFilters.length) {
                    // Only show complete matches
                    syncSearch(searchTerm, $(k));
                }
            });
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
        syncFiltering();
    };
    const initFilters = function() {
        // Update filter counts
        $('.filter')
            .each(function() {
                const $el = $(this);
                const matches = $(`.game[data-filters~="${$el.data('id')}"]`)
                    .length;
                const countText = `${matches}`;
                const matchText = (matches > 99)
                    ? `++`
                    : countText;
                $el
                    .find('.count')
                    .text(matchText)
                    .attr('title', countText);
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
            syncFiltering();
        }
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
                'change': function(v) { volumeReady = true; },
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
        initFilters();
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
    $(".game").on("click", function() {
        const item = $(this);
        const itemId = item.data('id');
        toggleScrim(true);
        launch(itemId);
    });
    $("#stop").on("click", function() {
        toggleScrim(true);
        stop();
    });
    $(".filter").on("click", function(e) {
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
        syncFiltering();
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
            syncFiltering();
        }
    });
    $("#search").on("input", function(e) {
        syncFiltering();
    });
});
