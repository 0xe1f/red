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
        if (!id) {
            return;
        }
        const $item = $(`.game[data-id="${id}"]`);
        if ($item.length) {
            $item.addClass('selected');
            $('#running .title').text(
                $item.children('.title').text()
            );
            $('#running').show();
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
                    $(`.filter[data-id~="${f}"`)
                        .addClass('selected');
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
                if (resp.title) {
                    updateSelection(resp.title);
                }
                updateVolume(resp.volume || 0);
            },
        );
        initFilters();
    };
    initialize();
    $(".game").on("click", function() {
        const item = $(this);
        const itemId = item.data('id');
        const payload = { 'id': itemId };
        $('#scrim').show();
        $.ajax({
            url: "launch",
            type: "POST",
            data: JSON.stringify(payload),
            contentType: "application/json; charset=utf-8",
            dataType: "json",
            success: function(){
                $('#scrim').hide();
                updateSelection(itemId);
            }
        });
    });
    $(".multi .filter").on("click", function(e) {
        const $item = $(this);
        if (!e.shiftKey && !$item.hasClass('selected')) {
            $item
                .parent()
                .find('.filter')
                .removeClass('selected');
        }
        $item.toggleClass('selected');
        syncFiltering();
    });
    $(".mutex .filter").on("click", function() {
        const $item = $(this);
        if (!$item.hasClass('selected')) {
            $item
                .parent()
                .find('.filter')
                .removeClass('selected');
        }
        $item.toggleClass('selected');
        syncFiltering();
    });
});
