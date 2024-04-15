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
    $.get(
        "query",
        function(resp) {
            if (resp.title) {
                updateSelection(resp.title);
            }
        },
    );
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
});
