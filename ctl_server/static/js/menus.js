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
 */

$().ready(function() {
    $(document).on('click', 'button.dropdown', function(e) {
        var topOffset = 0;
        var $button = $(this);
        var $menu = $('#' + $button.data('dropdown'));

        $('.menu').hide();
        $menu.show();

        if ($menu.hasClass('selectable')) {
            var $selected = $menu.find('.selected-menu-item');
            if ($selected.length) {
                topOffset += $selected.position().top;
                $selected.addClass('hovered').mouseout(function() {
                    $selected.removeClass('hovered').unbind('mouseout');
                });
            }
        } else {
            topOffset -= $button.outerHeight() - 1; // 1 pixel for the border
        }

        $menu.css({ 'top': $button.offset().top - topOffset });
        if ($button.offset().left + $menu.width() >= $(window).width())
            $menu.css({ 'right': $(window).width() - ($button.offset().left + $button.outerWidth()) });
        else
            $menu.css({ 'left': $button.offset().left });

        e.stopPropagation();
    });

    $(document).on('click', '.menu', function(e) {
        e.stopPropagation();
    });

    $(document).on('click', '.menu li', function(e) {
        var $item = $(this);
        var $menu = $item.closest('ul');

        var groupName = null;
        $.each($item.attr('class').split(/\s+/), function() {
            if (this.indexOf('group-') == 0) {
                groupName = this;
                return false;
            }
        });

        if (groupName) {
            $('.' + groupName).removeClass('selected-menu-item');
            $item.addClass('selected-menu-item');
        }

        if ($menu.hasClass('selectable')) {
            $('.dropdown').each(function() {
                var $dropdown = $(this);
                if ($dropdown.data('dropdown') == $menu.attr('id'))
                    $dropdown.text($item.text());
            });
        } else if ($item.hasClass('checkable')) {
            $item.toggleClass('checked', !$item.hasClass('checked'));
        }

        $menu.hide();

        $$menu.triggerClick($menu, $item);
    });

    $.fn.extend({
        selectItem: function(itemSelector) {
            return this.each(function () {
                var $menu = $(this);
                if ($menu.is('.menu.selectable')) {
                    var $selected = $(itemSelector); // $menu.find(itemSelector);

                    $menu.find('li').removeClass('selected-menu-item');
                    $selected.addClass('selected-menu-item');

                    $('.dropdown').each(function() {
                        var $dropdown = $(this);
                        if ($dropdown.data('dropdown') == $menu.attr('id'))
                            $dropdown.text($menu.find(itemSelector).text());
                    });
                }
            });
        },
        isSelected: function(itemSelector) {
            var $menu = $(this);
            if ($menu.is('.menu.selectable'))
                return $menu.find(itemSelector).hasClass('selected-menu-item');
            return false;
        },
        setChecked: function(checked) {
            return this.each(function () {
                var $item = $(this);
                if ($item.is('.menu li'))
                    $item.toggleClass('checked', checked);
            });
        },
        setTitle: function(title) {
            return this.each(function () {
                var $item = $(this);
                $item.find('span').text(title);
                if ($item.hasClass('selected-menu-item')) {
                    var $menu = $item.closest('ul');
                    $('.dropdown').each(function () {
                        var $dropdown = $(this);
                        if ($dropdown.data('dropdown') == $menu.attr('id'))
                            $dropdown.text(title);
                    });
                }
            });
        },
        openMenu: function(x, y, context) {
            return this.each(function () {
                var $menu = $(this);
                if ($menu.hasClass('menu')) {
                    $('.menu').hide();
                    $menu.css( { top: y, left: x }).data('context', context).show();
                }
            });
        }
    });
});

var $$menu = {
    clickCallback: null,
};

$$menu.click = function(callback) {
    this.clickCallback = callback;
};
$$menu.triggerClick = function($menu, $item) {
    if (this.clickCallback)
        this.clickCallback({
            '$menu': $menu,
            '$item': $item,
            'context': $menu.data('context'),
            'isChecked': $item.hasClass('checked'),
        });
};
$$menu.hideAll = function() {
    $('.menu').hide();
};
$("html")
    .click(function() { $$menu.hideAll(); });
$(document)
    .keyup(function(e) {
        switch (e.which) {
            case 27: // ESC
                $$menu.hideAll();
                break;
        }
    });
