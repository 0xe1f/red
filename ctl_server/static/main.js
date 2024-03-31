$(function() {
    $(".game").on("click", function() {
        const item = $(this);
        const payload = { 'id': item.data('id') };
        $.post(
            "action/launch",
            JSON.stringify(payload),
            function() { },
            "json",
        );
    });
});
