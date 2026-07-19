# Note this requires an internet connection
# Filename: pool_utils.py
#
# Shared HTTP-fetch-and-parse scaffolding for the plugin-pool scraping
# scripts (checkPool.py, getPool.py, getPlugins.py). Each of those scripts
# fetches the pool's HTML plugin listing, parses out (repo-url, plugin-name)
# pairs from the <table>/<a href=...> markup, and fans work out across
# per-site pages with a ThreadPoolExecutor. This module holds that common
# logic; what each script DOES with a parsed (href, name) pair -- comparing
# against a local checkout, cloning, filtering by pipeline plugin list, etc
# -- stays in the script as a handler callback.

import urllib.request
from concurrent.futures import ThreadPoolExecutor

BASE_URL = "http://biorg.cis.fiu.edu/pluma/"


def fetch_page(path, timeout=15):
    """Fetch "<BASE_URL><path>" and return its decoded page source."""
    response = urllib.request.urlopen(BASE_URL + path, timeout=timeout)
    return str(response.read())


def iter_link_entries(page_source):
    """Walk every <table>...</table> block in page_source and yield the
    split('>') parts of each <a href=...>...</a> tag's contents.

    For a well-formed entry this yields a 2-element list:
    [ '"<repo-url>"', '<plugin-name>' ]. Mirrors the original per-script
    parsing loops exactly (including tolerating malformed/short entries,
    which callers may choose to filter on length).
    """
    while page_source.find("</table>") != -1:
        plugin_table = page_source[page_source.find("<table "):page_source.find("</table>")]
        plugins = plugin_table.split("<tr>")
        for plugin in plugins:
            while plugin.find("</a>") != -1:
                content = plugin[plugin.find("<a href="):plugin.find("</a>")]
                content = content.replace('<a href=', '')
                data = content.split('>')
                yield data
                plugin = plugin[plugin.find("</a>") + 1:]
        page_source = page_source[page_source.find("</table>") + 1:]


def discover_websites(index_page="plugins.html", timeout=15):
    """Fetch the top-level pool index page and return the set of per-site
    page paths linked from it (the href of every <a> tag found)."""
    page_source = fetch_page(index_page, timeout=timeout)
    websites = set()
    for data in iter_link_entries(page_source):
        websites.add(data[0][1:len(data[0]) - 1])
    return websites


def scrape_pool(websites, handler, timeout=15, max_workers=8):
    """Fetch each website's page concurrently, parse out its (href, name)
    plugin entries, and call handler(website, entries) with the result.

    entries is a list of (href, name) tuples -- href still quoted, e.g.
    '"http://example.com/repo"' -- for every <a> tag whose split('>')
    yielded exactly 2 parts (i.e. a well-formed plugin link).
    """
    def _process(website):
        page_source = fetch_page(website, timeout=timeout)
        entries = [
            (data[0], data[1])
            for data in iter_link_entries(page_source)
            if len(data) == 2
        ]
        handler(website, entries)

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        list(executor.map(_process, websites))
