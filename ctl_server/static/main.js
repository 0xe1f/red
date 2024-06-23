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
        $('#running .title').toggleClass('valid', !!id);
        if (!id) {
            $('#title_label').css('visibility', 'hidden');
            $('#running .title').text('No Game Selected');
            return;
        }
        const $item = $(`.game[data-id="${id}"]`);
        $('#title_label').css('visibility', 'visible');
        if ($item.length) {
            $item.addClass('selected');
            $('#running .title').text(
                $item.children('.title').text()
            );
        }
    };
    const syncFiltering = function() {
        const $selectedFilters = $('.filter.selected');
        if (!$selectedFilters.length) {
            $('.game').show();
            location.hash = '';
            return;
        }
        var appliedSymbols = [];
        const matchMap = new Map();
        $('.game').hide();
        $selectedFilters
            .each(function(i, obj) {
                const filter = $(obj).data('id');
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
                    $(k).show();
                }
            });
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
                $('#scrim').hide();
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
                $('#scrim').hide();
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
    initialize();
    $(".game").on("click", function() {
        const item = $(this);
        const itemId = item.data('id');
        $('#scrim').show();
        launch(itemId);
    });
    $("#stop").on("click", function() {
        $('#scrim').show();
        stop();
    });
    $(".filter").on("click", function(e) {
        const $item = $(this);
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
    $(".filters .label").on("click", function(e) {
        const $item = $(this);
        const $filters = $item
            .closest('.filters');
        if (!$filters.hasClass('active')) {
            return;
        }

        $filters
            .find('.filter')
            .removeClass('selected');
        $filters
            .removeClass('active');

        syncFiltering();
    });
});
