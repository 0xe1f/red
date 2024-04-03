$(function() {
    $(".game").on("click", function() {
        const item = $(this);
        const payload = { 'id': item.data('id') };
        $.post(
            "launch",
            JSON.stringify(payload),
            function() {
                $('.game').removeClass('selected');
                item.addClass('selected');
            },
            "json",
        );
    });
});
