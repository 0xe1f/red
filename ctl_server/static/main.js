$(function() {
    $.get(
        "query",
        function(resp) {
            if (resp.title) {
                $('.game[data-id="' + resp.title + '"]').addClass('selected');
            }
        },
    );
    $(".game").on("click", function() {
        const item = $(this);
        const payload = { 'id': item.data('id') };
        $('#scrim').show();
        $.post(
            "launch",
            JSON.stringify(payload),
            function() {
                $('#scrim').hide();
                $('.game.selected').removeClass('selected');
                item.addClass('selected');
            },
            "json",
        );
    });
});
