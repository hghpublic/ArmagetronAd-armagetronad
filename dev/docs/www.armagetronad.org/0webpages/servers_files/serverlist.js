/* serverlist.js - Lets you browse armagetronad servers
 * Copyright (C) 2011  Yann Kaiser
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
jQuery(function(){

if( !window.serverlist )
    window.serverlist = {};

jQuery.ajaxSetup({'timeout': 10000});

var serverlist_url, dark_threshold;
/* Nelg's server list */
serverlist_url = "https://corsapi.armanelgtron.tk/servers_link/serverlist.php";
/* serverlist_url = "serverxml.php"; */
/* serverlist_url = "http://simamo.de/~manuel/serverlist/serverlist.xml"; */

if( serverlist.filter_colors === undefined)
    serverlist.filter_colors = false;
dark_threshold = {r: 128, g: 128, b: 128, overall: 179};

var div, ul, filters, servers;
div = jQuery("#serverlist");
ul = jQuery("<ul>");
filters = jQuery("<div>").addClass('filters');

servers = [];

div.empty();
div.append(filters);
div.append(ul);

function is_just_created()
{
    if(ul.children().length)
    {
        return false;
    }
    else
    {
        return true;
    }
}

function text_el(text)
{
    return jQuery(document.createTextNode(text));
}

function is_dark(r, g, b)
{
    return (
        (
            r < dark_threshold.r
            && g < dark_threshold.g
            && b < dark_threshold.b
        )
        || r + g + b < dark_threshold.overall
    )
}

// Parses color codes; Used right about everywhere.
function pcc(text, eltype)
{
    var children = false;
    if( eltype === undefined )
    {
        eltype = 'span';
        children = true;
    }
    var ret = jQuery(document.createElement(eltype));
    if( serverlist.filter_colors )
    {
        ret.append(fcc(text));
        return ret;
    }
    var first = true;
    var last_i = 0;
    text.replace(
        /0x(([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})|RESETT|.{6})(.*?)(?=0x(?:.{6}|RESETT)|$)/g,
        function(sub, hex, r_, g_, b_, colored, i){
            if( first && i )
            {
                ret.append(text_el(text.slice(0, i)));
            }
            first = false;
            var el;
            if(hex == 'RESETT' || !r_ || !g_ || !b_)
            {
                el = text_el(colored);
            }
            else
            {
                var el = jQuery('<span class="color-code" style="color: #' + hex + '">');
                var r, g, b;
                r = parseInt(r_, 16);
                g = parseInt(g_, 16);
                b = parseInt(b_, 16);
                if(is_dark(r, g, b))
                {
                    el.addClass('dark');
                }
                el.text(colored);
            }
            ret.append(el);
            last_i = i + sub.length;
            return sub;
        });
    ret.append(text_el(text.slice(last_i)))
    if( children )
        return jQuery(ret).children();
    return jQuery(ret);
}

function fcc(text)
{
    return text.replace(/0x(?:.{6})/g, '');
    // return text.replace(/0x(?:[0-9a-f]{6}|RESETT)/g, '');
}

function transliterate(text, search, replace, fallback)
{
    var ret = '';
    for(var i = text.length - 1; i >=0; i--)
    {
        var c = text[i];
        var j = search.indexOf(c);

        if(j > -1)
        {
            ret = replace[j] + ret;
        }
        else if(fallback !== undefined)
        {
            ret = fallback + ret;
        }
        else
        {
            ret = c + ret;
        }
    }
    return ret;
}
function sortname(text)
{
    return transliterate(
        fcc(text).toLowerCase(),
        'ÀÅÑÖÙÝßàåèëìïðöùü°$+×01234567abcdefghijklmnopqrstuvwxyz',
        'aanouysaaeeiioouuostxoizeasgtabcdefghijklmnopqrstuvwxyz',
        ''
        );
}

var selected = jQuery();
var servers_by_address = {};

function create_li(address)
{
    var server, li,
        playercounts, numplayers, maxplayers;
    li = jQuery('<li>')
        .addClass('collapsed')
        .click(function(ev){
            if(this != selected[0])
            {
                selected.addClass('collapsed');
            }
            selected = jQuery(this).removeClass('collapsed');
        });

    li.append('<span class="overlay"></span>');

    li.attr('id', 'server-' + address);

    jQuery('<strong>')
        .appendTo(li);

    playercounts = jQuery('<span>')
        .addClass('playercounts');
    numplayers = jQuery('<span>')
        .addClass('numplayers');
    maxplayers = jQuery('<span>')
        .addClass('maxplayers');

    playercounts
        .append(numplayers, maxplayers)
        .appendTo(li);

    jQuery('<a>')
        .addClass('join-link')
        .text('armagetronad://' + address)
        .attr('href', 'armagetronad://' + address)
        .appendTo(li);

    jQuery('<ul>')
        .appendTo(li);

    jQuery('<p>')
        .addClass('description')
        .appendTo(li);

    servers_by_address[address] = li;
    return li.appendTo(ul);
}
function update_existing_server(li, data)
{
    li.data(data);

    pcc(data.raw.name, 'strong')
        .replaceAll(jQuery('> strong', li))
        .click(function(ev){
            var parent = jQuery(this).parent();
            if(selected[0] != parent[0])
            {
                selected.addClass('collapsed');
            }
            selected = parent.toggleClass('collapsed');
            ev.preventDefault()
            return false;
        });
        ;

    jQuery('.numplayers', li).text(data.numplayers);
    jQuery('.maxplayers', li).text(data.maxplayers);

    var players = jQuery('> ul', li).empty();
    for(i in data.players)
    {
        pcc(data.players[i], 'li')
            .appendTo(players);
    }

    pcc(data.raw.description, 'p')
        .replaceAll(jQuery('> p', li))
        .addClass('description')
        .append(text_el(' '))
        .append(jQuery('<a>')
                    .addClass('website')
                    .text(data.website)
                    .attr('href', data.website)
                )
        .append(text_el(' '))
        .append(jQuery('<span>')
                    .addClass('version')
                    .text(data.version)
                )

}
function update_server(i, e){
    var server;
    server = jQuery(e);

    var data = {};

    data.address = server.attr('ip')
    if(server.attr('port') != "4534")
        data.address += ':' + server.attr('port');

    var li = servers_by_address[data.address];

    data.raw = {
        'name': server.attr('name'),
        'description': server.attr('description'),
        'numplayers': server.attr('numplayers'),
        'maxplayers': server.attr('maxplayers'),
        'website': server.attr('url')
    };
    data.name = fcc(data.raw.name);
    data.sortname = sortname(data.raw.name);
    data.numplayers = Number(data.raw.numplayers);
    data.maxplayers = Number(data.raw.maxplayers);
    data.description = fcc(data.raw.description);
    data.description_ = sortname(data.raw.description);
    data.website = fcc(data.raw.website);
    data.version = server.attr('version');

    data.players = [];
    playersort = [];
    jQuery('Player', server).each(function (i, e){
        var player = jQuery(e);
        data.players.push(fcc(player.attr('name')));
        playersort.push(sortname(player.attr('name')));
    });
    data.players_ = playersort.join(' ');

    if( li === undefined )
    {
        li = create_li(data.address);
    }
    update_existing_server(li, data);

    return li[0];
}

function tell_about_help(time) {
    show_status('Press F1 for keyboard shortcuts');

    if( time > 100000 ) return;

    window.setTimeout(tell_about_help, time + 20000, time + 20000);
}

function display_from_feed(data) {
    var just_created = is_just_created();
    var servers = jQuery('Server', data).map(update_server);

    window.setTimeout(function(){
        previous_filter_opts = {};
        filter(false);

        if( just_created )
        {
            hide_status(loading_id);
                window.setTimeout(function(){
                        tell_about_help(0);
                    }, 5000);
        }
        else
        {
            ul.children().not(servers).remove();
            show_status("Refreshed servers");
        }
    }, 0);
    window.setTimeout(get_feed, 60000);
}

function on_error(jqXHR, textStatus, errorThrown) {
    show_status('Error while fetching feed. Trying again in 30 seconds.');
    window.setTimeout(get_feed, 30000);
}

function get_feed(){
    jQuery.get(serverlist_url, display_from_feed, 'xml').error(on_error);
}

// Bookmarks
var bookmarks = [];
function is_bookmark(address)
{
    return bookmarks.indexOf(address) >= 0;
}
function is_bookmarked(el)
{
    if( el === undefined )
        el = selected;

    return is_bookmark(el.data('address'));
}
function add_bookmark(el)
{
    if( el === undefined )
        el = selected;

    if( !is_bookmarked(el) )
        bookmarks.push(el.data('address'));
}
function del_bookmark(el)
{
    if( el === undefined )
        el = selected;

    if( is_bookmarked(el) )
        delete bookmarks[bookmarks.indexOf(el.data('address'))];
}
function toggle_bookmark(el)
{
    if(is_bookmarked(el))
        del_bookmark(el);
    else
        add_bookmark(el);
}
jQuery(document).jkey('alt+b', function(){
    toggle_bookmark();
    previous_filter_opts = {};
    filter(false);
});

// Sorting
function cmp_is_bookmarked(a, b)
{
    return is_bookmark(a.address) - is_bookmark(b.address);
}
function cmp_is_populated(a, b)
{
    return (a.players.length > 0) - (b.players.length > 0);
}
function cmp_by_users(a, b)
{
    return a.players.length - b.players.length;
}
function strcmp(a, b)
{
    if( a > b ) return 1;
    if( a < b ) return -1;
    return 0;
}
function cmp_by_name(a, b)
{
    return strcmp(a.sortname, b.sortname);
}
function cmp_by_addr(a, b)
{
    return strcmp(a.address, b.address);
}

function reverse(cmp)
{
    function _reverse(a, b)
    {
        return -cmp(a, b);
    }
    return _reverse;
}
function multicmp()
{
    var cmps = arguments;
    function _multicmp(a, b)
    {
        var result;
        for(var i = 0;i < cmps.length; i++)
        {
            result = cmps[i](a, b);
            if( result != 0 )
                return result;
        }
        return 0;
    }
    return _multicmp;
}

var cmp_fns = {
        'User count': multicmp(
            reverse(multicmp(cmp_is_populated, cmp_is_bookmarked, cmp_by_users)),
            cmp_by_name, cmp_by_addr),
        'Server name': multicmp(
            reverse(cmp_is_bookmarked),
            cmp_by_name,
            cmp_by_addr
            )
    };
window.cmp_fns = cmp_fns;
function sort(cmp)
{
    var first_is_selected = selected.is(jQuery('> .filter-match', ul).first());
    if( cmp === undefined )
    {
        cmp = cmp_fns[filter_opts.sort];
    }

    var list = jQuery('> li', ul);

    function cmp_(a, b)
    {
        return cmp(jQuery.data(a), jQuery.data(b));
    }

    list.sort(cmp_);
    put_in_order(list);
    reselect(first_is_selected);
}

function put_in_order(list)
{
    list.each(function(el){
        ul[0].appendChild(list[el]);
    });
}

function reselect(selected_was_first)
{
    if( !selected.length
        || selected_was_first || selected.hasClass('filter-nomatch') )
    {
        jQuery('> li.filter-match', ul).first().click();
    }
}
jQuery(document).jkey('alt+a', function(){ reselect(true); })

function scrollToCenter(el)
{
    var wheight = jQuery(window).height();
    var dheight = jQuery(document).height();

    var elpos = el.offset().top + el.outerHeight() / 2;

    var newScrollTop = elpos - wheight / 2;
    if(newScrollTop < 0) newScrollTop = 0;
    if(newScrollTop > dheight - wheight) newScrollTop = dheight - wheight;

    /* jQuery('html,body').clearQueue().animate({
            scrollTop: newScrollTop
        }); */ // CPU goes through the roof, plus a special easing function
               // needs to be done for this to look smooth

    jQuery(window).scrollTop(newScrollTop);
}

function kb_up(ev){
    var current = jQuery('> li.filter-match:not(.collapsed)', ul);
    if(current.length != 1)
    {
        current = ul.children('.filter-match').first();
    }

    var next;
    if(jQuery('> li.filter-match:first', ul).filter(current).length)
    {
        next = ul.children('.filter-match').last();
    }
    else
    {
        next = current.prevAll('.filter-match').first();
    }

    if(!next.length) return false;
    next.click();
    scrollToCenter(next);
}

function kb_down(ev){
    var next;
    if(selected.length != 1)
    {
        next = ul.children('.filter-match').last();
    }
    else if(jQuery('> li.filter-match:last', ul).filter(selected).length)
    {
        next = ul.children('.filter-match').first();
    }
    else
    {
        next = selected.nextAll('.filter-match').first();
    }

    if(!next.length) return false;
    next.click();
    scrollToCenter(next);
}

jQuery(document).jkey('up', kb_up);
jQuery(document).jkey('down', kb_down);

function kb_enter(){
    window.location.href = 'armagetronad://' + selected.data('address');
}
jQuery(document).jkey('enter', kb_enter);

var neutral_filter_opts = {
    'empty': true, 'populated': true, 'full':true,
    'text': '',
    're_players': null,
    're_name': null,
    'sort': 'User count'
    };
var previous_filter_opts = {};
var default_filter_opts = jQuery.extend({}, neutral_filter_opts);
if( jQuery.isFunction(serverlist.set_default_filters) )
{
    serverlist.set_default_filters(default_filter_opts);
}
else
{
    default_filter_opts.empty = false;
}
var filter_opts = jQuery.extend({}, default_filter_opts);
var own_filters = false;
function match_players_filter(players, re)
{
    for(var player_ in players)
    {
        if( players[player_].match(re) )
        {
            return true;
        }
    }
    return false;
}
function match_filter(data)
{
    return !(
            !( filter_opts.empty || data.players.length )
         || !( filter_opts.populated || data.players.length < 1 )
         || !( filter_opts.full || data.players.length < data.maxplayers )
         || !( filter_opts.text_.length < 1 ||
                data.sortname.indexOf(filter_opts.text_)
                + data.description_.indexOf(filter_opts.text_)
                + data.players_.indexOf(filter_opts.text_)
                > -3 )
         || !( filter_opts.re_name === null
              || data.name.match(filter_opts.re_name) !== null
              || data.description.match(filter_opts.re_name) !== null
              )
         || !( filter_opts.re_players === null
              || match_players_filter(data.players, filter_opts.re_players)
              )
        );
}
function filters_equal(a, b)
{
    for(var key in a)
    {
        if(key.substr(-1) != '_' && a[key] != b[key])
        {
            if( a[key] instanceof RegExp && b[key] instanceof RegExp )
            {
                if( a[key].source != b[key].source
                        || a[key].flags != b[key].flags )
                    return false;
            }
            else
                return false;
        }
    }

    for(var key in b)
        if(key.substr(-1) != '_' && a[key] === undefined)
            return false;

    return true;
}
function filter_server(index, element)
{
    var el = jQuery(element);
    var matches = match_filter(el.data());
    el.toggleClass('filter-nomatch', !matches);
    el.toggleClass('filter-match', matches);
    el.toggleClass('bookmark', is_bookmarked(el));
}
var show_no_match_id = 0;
function filter(cause_own_filters)
{
    var first_is_selected = selected.is(ul.children().first());

    var searchees = jQuery('> li', ul);

    if( !searchees.length ){
        return;
    }

    if( filter_opts.sort != previous_filter_opts.sort )
    {
        sort();
        previous_filter_opts.sort = filter_opts.sort;
        store_prefs();
    }
    if( filters_equal(filter_opts, previous_filter_opts) )
        return;

    filter_opts.text_ = sortname(filter_opts.text);
    previous_filter_opts = jQuery.extend({}, filter_opts);
    store_prefs();

    var is_neutral = filters_equal(filter_opts, neutral_filter_opts);
    ul.toggleClass('filter-active', !is_neutral);
    clearSearch.toggleClass('hide', is_neutral);

    update_options();

    if( cause_own_filters !== false )
    {
        own_filters = true;
    }

    searchees.each(filter_server);

    if( searchees.length && !jQuery('> .filter-match', ul).length )
    {
        window.clearTimeout(show_no_match_id);
        show_no_match_id = window.setTimeout(function(){
            show_status("No matches for the current filters");
        }, 1000);
    }
    reselect(first_is_selected);
}
function store_prefs(setter, filter)
{
    if( setter === undefined )
    {
        if( !window.localStorage ) return;

        setter = function(prop, val){
            localStorage.setItem('serverlist__' + prop, val);
        };
        if(!own_filters) return;
    }

    if( filter === undefined )
    {
        filter = filter_opts;
    }

    for(var prop in filter)
    {
        if( prop.substr(-1) === '_' )
            continue;

        var val = filter[prop];
        if( val === null )
            val = '';
        else if( val instanceof RegExp )
            val = val.source;

        setter('filter_' + prop, String(val));
    }

    setter('bookmarks', bookmarks.join(' '));
}
function make_hash()
{
    var prefs = {};

    store_prefs(function(prop, val){
        prefs[prop] = val.replace('\\', '\\\\').replace(';', '\\;');
    });

    var default_prefs = {};
    store_prefs(function(prop, val){
        default_prefs[prop] = val.replace('\\', '\\\\').replace(';', '\\;');
    }, default_filter_opts);

    var hashes = [];
    for( var key in prefs )
    {
        if( prefs[key] != default_prefs[key] )
        {
            hashes.push(key + '=' + prefs[key]);
        }
    }

    document.location.hash = '#' + encodeURIComponent(hashes.join(';'));
    show_status("Custom link now in the address bar");
}
jQuery(document).jkey('alt+l', make_hash);
function restore_prefs(getter)
{
    if( getter === undefined )
    {
        if( !window.localStorage ) return;
        getter = function(prop){
            return localStorage.getItem('serverlist__' + prop);
        };
    }
    else
    {
        own_filters = false;
    }

    for(var prop in filter_opts)
    {
        try
        {
            var val = getter('filter_' + prop);

            if( val === null )
                continue;

            if( prop == 're_name' || prop == 're_players' )
            {
                if( val.length )
                    val = RegExp(val, 'i');
                else
                    val = null;
            }
            else if( prop == 'empty' || prop == 'populated' || prop == 'full' )
            {
                val = val == 'true';
            }

            filter_opts[prop] = val;
        }
        catch(err)
        {
        }
    }

    var bookmarks_ = getter('bookmarks');
    if( bookmarks_ !== null )
    {
        bookmarks = bookmarks_.split(' ');
    }
}
function read_hash()
{
    var hashes_string = decodeURIComponent(
        document.location.hash.substr(1));
    console.log(hashes_string);

    // Can't do lookbehinds? reverse everything!
    hashes = hashes_string.split('').reverse()
        .join('').split(/;(?!\\(?:\\\\)*[^\\])/);

    var prefs = {};

    for(var i in hashes)
    {
        try
        {
            var hash = hashes[i].split('').reverse().join('')
                .replace('\\\\', '\\');
            var keyval = hash.split('=', 2);
            if(keyval.length > 1)
                prefs[keyval[0]] = keyval[1];
        }
        catch(err){}
    }

    restore_prefs(function(prop){
        var ret = prefs[prop];
        if( ret === undefined )
            ret = null;
        return ret;
    });
}

var statusBar = jQuery('<span>')
    .addClass('status');
var status1 = jQuery('<span>');
var status2 = jQuery('<span>');
statusBar.append(status1, status2);
var last_used_status = false;
var status_id = -1;
function show_status(text, time)
{
    if( time === undefined )
    {
        time = 2000;
    }
    if(last_used_status)
    {
        status1.text(text);
    }
    else
    {
        status2.text(text);
    }
    status1.toggleClass('hide', !last_used_status);
    status2.toggleClass('hide', last_used_status);
    last_used_status = !last_used_status;

    searchField.addClass('hide');
    searchLabel.addClass('hide');

    var status_id_ = window.setTimeout(function(){
            hide_status(status_id_);
        }, time);
    status_id = status_id_
    return status_id;
}
function hide_status(id)
{
    if(id == status_id || id < 0)
    {
        window.clearTimeout(id);

        searchLabel.toggleClass('hide', searchField.val().length > 0);
        clearSearch.toggleClass('hide', filters_equal(filter_opts, neutral_filter_opts));
        searchField.toggleClass('hide', searchField.val().length == 0);
        status1.addClass('hide');
        status2.addClass('hide');
    }
}

var searchBar = jQuery('<div>')
    .addClass('search-bar');
filters.append(searchBar);

var searchBarButtons = jQuery('<div>')
    .addClass('buttons');
searchBar.append(searchBarButtons);

var searchLabel = jQuery('<label>')
    .text('Type to search...')
    .addClass('search-label')
    .addClass('hide')
    .attr('for', 'serverlistsearch');
searchBar.append(statusBar);
searchBar.append(searchLabel);

var clearSearch = jQuery('<a>')
    .text('×')
    .addClass('search-clear')
    .addClass('hide')
    .click(function(){
        filter_opts = jQuery.extend({}, neutral_filter_opts);
        filter(false);
    });
searchBarButtons.append(clearSearch);
jQuery(document).jkey('alt+r', function(){ clearSearch.click(); });
jQuery(document).jkey('alt+d', function(){
        filter_opts = jQuery.extend({}, default_filter_opts);
        filter();
    });

var prev_rawsearch = '';
var searchField = jQuery('<input>')
    .attr('spellcheck', 'false')
    .attr('id', 'serverlistsearch')
    .addClass('hide');
searchField.keyup(function (){
    if( prev_rawsearch == searchField.val() ) return;
    prev_rawsearch = searchField.val();
    hide_status(-1);
    filter_opts.text = searchField.val();
    filter(false);
    });
searchField.focus(function (){
    searchLabel.text("Type to search...");
    });
searchField.blur(function (){
    searchLabel.text("Click to search...");
    });
searchField.click(function(){ searchField.keyup(); });
searchBar.append(searchField);
jQuery(document).jkey('alt+s', function(){ searchField.focus(); });

var moreLink = jQuery('<a>')
    .addClass('more-link')
    .text("More options")
    .click(function(){
        var show = (
            options.hasClass('hide')
            || moreLink.offset().top < jQuery(window).scrollTop() );
        options.toggleClass('hide', !show);
        moreLink.toggleClass('active', show);
        if( show && moreLink.offset().top < jQuery(window).scrollTop() )
        {
            jQuery('html, body').animate({scrollTop: moreLink.offset().top}, 500);
        }
    });
searchBarButtons.append(moreLink);

jQuery(document).jkey('alt+o', function(){ moreLink.click(); });
jQuery(document).jkey('alt+f', function(){ moreLink.click(); });

var options = jQuery('<div>')
    .addClass('options')
    .addClass('hide');
filters.append(options);

options_by_prop = {};
options_by_prop.text = searchField;

function try_re(re)
{
    if( !re.length )
        return null;

    var ret;
    try
    {
        ret = RegExp(re, 'i');
    }
    catch(err)
    {
        return false;
    }
    return ret;
}

var serverRe = jQuery('<input>')
    .attr('spellcheck', 'false')
    .keyup(function(){
        var re = try_re(serverRe.val());

        if( re === false )
        {
            serverRe.addClass('error');
        }
        else
        {
            serverRe.removeClass('error');
            filter_opts.re_name = re;
            filter();
        }
    })
    .click(function(){ jQuery(this).keyup() });
options_by_prop.re_name = serverRe;

jQuery('<div>')
    .addClass('filters-re-server')
    .append('<h3>Server Regexp</h3>')
    .append(serverRe)
    .appendTo(options);

var playersRe = jQuery('<input>')
    .attr('spellcheck', 'false')
    .keyup(function(){
        var re = try_re(playersRe.val());

        if( re === false )
        {
            playersRe.addClass('error');
        }
        else
        {
            playersRe.removeClass('error');
            filter_opts.re_players = re;
            filter();
        }
    })
    .click(function(){ jQuery(this).keyup() });

options_by_prop.re_players = playersRe;

jQuery('<div>')
    .addClass('filters-re-players')
    .append('<h3>Player Regexp</h3>')
    .append(playersRe)
    .appendTo(options);

function update_options()
{
    var field, type;
    for(var prop in options_by_prop)
    {
        field = options_by_prop[prop];
        type = field.attr('type');
        val = filter_opts[prop];
        if( type == 'checkbox' )
        {
            field.prop('checked', val);
        }
        else if( type == 'radio' )
        {
            field.each(function(n, el){
                var jel = jQuery(el);
                jel.prop('checked', jel.val() == val);
            });
        }
        else
        {
            if( val instanceof RegExp )
            {
                val = val.source;
            }
            field.val(val);
        }
    }
}
function make_checkbox(id, text, prop, type, group)
{
    if( type === undefined )
    {
        type = 'checkbox';
    }

    var ret = jQuery('<input>')
        .attr('id', id)
        .attr('type', type);

    var setter;
    if( jQuery.isFunction(prop) )
    {
        setter = prop;
    }
    else
    {
        setter = function(val){
            filter_opts[prop] = val;
            filter();
        };
        options_by_prop[prop] = ret;
    }

    ret.change(function(){
        var checked = ret.prop('checked');
        if( type == "checkbox" || checked )
        {
            setter(checked);
        }
    });

    if( type == 'radio' )
    {
        ret.attr('name', group)
           .val(text);
    }

    var label = jQuery('<label>')
        .attr('for', id)
        .text(text);

    // many browsers don't support styling the checkbox into a heightless line-breaker. <br>, this is your mission.
    return ret.after(label).after('<br>');
}

var sorting = jQuery('<div>')
    .addClass('filters-sort');
sorting.append(jQuery('<h3>').text('Sort criteria'));
jQuery.each(cmp_fns, function(fn){
    make_checkbox(
        fn.toLowerCase().replace(' ', '-'),
        fn,
        function(checked){
            filter_opts.sort = fn;
            filter();
        },
        'radio',
        'sort')
        .appendTo(sorting);
});
options.append(sorting);
options_by_prop.sort = jQuery('#serverlist .options .filters-sort input');

var filterBoxes = jQuery('<div>')
    .addClass('filters-check');
filterBoxes.append(jQuery('<h3>').text('Filters'));

function is_checked(el)
{
    return el.filter(':checked').length;
}

var hideEmpty =
    make_checkbox('show-empty', "Show empty servers", 'empty')
    .appendTo(filterBoxes);

var hidePopulated =
    make_checkbox('show-populated', "Show populated servers", 'populated')
    .appendTo(filterBoxes);

var hideFull =
    make_checkbox('show-full', "Show full servers", 'full')
    .appendTo(filterBoxes);

options.append(filterBoxes);

var help = jQuery(
"<div>"
+"<h3>Keyboard shortcuts</h3>"
+"<dl>"
+"<dt>F1</dt><dd>Show or hide this reference</dd>"
+"<dt>↑/↓</dt><dd>Scroll up or down in the list</dd>"
+"<dt>enter</dt><dd>Enter the currently-selected server</dd>"
+"<dt>alt+a</dt><dd>Select the first server</dd>"
+"<dt>alt+b</dt><dd>(Un-)bookmark a server</dd>"
+"<dt>alt+s</dt><dd>Select the search field</dd>"
+"<dt>alt+f</dt><dd>Show or hide filter options</dd>"
+"<dt>alt+r</dt><dd>Reset filters and show all servers</dd>"
+"<dt>alt+d</dt><dd>Reset filters to default values</dd>"
+"<dt>alt+l</dt><dd>Create a link for your filters</dd>"
+"</dl></div>")
    .addClass('help')
    .addClass('hide');
div.append(help);
jQuery(document).jkey('f1', function(){
    help.toggleClass('hide');
});

var loading_id = show_status("Loading...", 60000);
get_feed();
if( document.location.hash.length > 1 )
{
    read_hash();
}
else
{
    restore_prefs();
}

window.serverlist = jQuery.extend({
        'filters': filter_opts,
        'sorts': cmp_fns,
        'update': filter,
        'set_status': show_status,
        'hide_status': hide_status,
        'bookmarks': bookmarks,
        'servers': servers_by_address,
        'cmp': {
            'reverse': reverse,
            'multicmp': multicmp,
            'cmp_is_bookmarked': cmp_is_bookmarked,
            'cmp_is_populated': cmp_is_populated,
            'cmp_by_users': cmp_by_users,
            'strcmp': strcmp,
            'cmp_by_name': cmp_by_name,
            'cmp_by_addr': cmp_by_addr,
            }
    }, window.serverlist);

});
