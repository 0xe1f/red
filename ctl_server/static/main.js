$(function() {
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
            return;
        }
        const matchMap = new Map();
        $('.game').hide();
        $selectedFilters
            .each(function(i, obj) {
                const filter = $(obj).data('id');
                $(`.game[data-filters~="${filter}"]`)
                    .toArray()
                    .forEach(element => {
                        matchMap.set(element, (matchMap.get(element) ?? 0) + 1);
                    });
                });
        matchMap
            .forEach((v, k) => {
                if (v == $selectedFilters.length) {
                    // Only show complete matches
                    $(k).show();
                }
            });
};
    const setVolume = function(vol) {
        $.post(
            "volume",
            JSON.stringify({ 'volume': vol }),
            function() { },
            "json",
        );
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
    };
    initialize();
    $(".game").on("click", function() {
        const item = $(this);
        const itemId = item.data('id');
        const payload = { 'id': itemId };
        $('#scrim').show();
        $.post(
            "launch",
            JSON.stringify(payload),
            function() {
                $('#scrim').hide();
                updateSelection(itemId);
            },
            "json",
        );
    });
    $(".filter").on("click", function() {
        const $item = $(this);
        $item.toggleClass('selected');
        syncFiltering();
    });
});
