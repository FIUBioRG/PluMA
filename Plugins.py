#!/usr/bin/env python
'''Module for dowwnloading and checking plugins from the public PluMA plugin pool'''
import os
from os import path
import sys
import argparse
import logging
from urllib import request
import curses

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('plugins')

BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE = range(8)
EPS=1e-8

def has_colours(stream):
    '''Following tutorial from Python cookbook: #475186'''
    if not hasattr(stream, 'isatty'):
        return False
    if not stream.isatty():
        return False # auto color only on TTYs
    try:
        curses.setupterm()
        return curses.tigetnum('colors') > 2
    except Exception:
        # guess false in case of error
        return False

has_colors = has_colours(sys.stdout)

def normalprintout(text, colour=WHITE):
    '''Print a normal line of text'''
    if has_colors:
        seq = '\x1b[1;%dm' % (30+colour) + text + '\x1b[0m'
        sys.stdout.write('{}'.format(seq))
    else:
        sys.stdout.write('{}'.format(text))

def printout(text, colour=WHITE):
    '''Print out a line of text with formatting'''
    if has_colors:
        seq = '\x1b[1;%dm' % (30+colour) + text + '\x1b[0m'
        sys.stdout.write('{:>50}'.format(seq))
    else:
        sys.stdout.write('{:>50}'.format(text))

def get_categories():
    '''Download the list of plugin catergories from the main BioRG website tables'''
    pool = set()
    urls = set()

    with request.urlopen('https://biorg.cis.fiu.edu/pluma/plugins.html') as response:
        page_source = str(response.read())

        while page_source.find('</table>') != -1:
            category_table = page_source[page_source.find('<table '): page_source.find('</table>')]
            rows = category_table.split('<tr>')

            for row in rows:
                columns = row.split('<td ')
                for column in columns:
                    content = column[column.find('<a href='): column.find('</a>')]
                    content = content.replace('<a href=', '')
                    content = content.replace('<b', '')
                    content = content.replace('</b', '')
                    content = content.replace('"', '')
                    data = content.split('>')

                    if data[0] == '':
                        continue
                    elif len(data) == 4:
                        urls.add(data[0])
                        pool.add(data[2])

                row = row[row.find('</a>') + 1:]

            page_source = page_source[page_source.find('</table>') + 1:]

    return pool, urls

def get_plugins():
    '''Download the list of plugin from the category website tables'''
    pool = list()
    plugin_urls = list()
    categories, urls = get_categories()

    for category in urls:
        with request.urlopen('https://biorg.cis.fiu.edu/pluma/' + category) as response:
            page_source = str(response.read())

            table = page_source[page_source.find('<table '): page_source.find('</table')]
            rows = table.split('<tr>')

            for row in rows:
                content = row[row.find('<a href='): row.find('</a>')]
                data = content.split('>')
                data[0] = data[0].replace('<a href=', '')
                data[0] = data[0].replace('"', '')

                logger.debug(data[0])

                if data[0] == '':
                    continue
                elif len(data) == 2:
                    plugin_urls.append(data[0])
                    pool.append(data[1])

    return pool, plugin_urls

def download_plugins():
    '''Download plugins to a directory'''
    plugins, urls = get_plugins()

    for i in range(len(urls)):
        if path.exists(path.join(args.dirPath, plugins[i])):
            print('Plugin ' + plugins[i] + ' already installed.')
        else:
            os.system('git clone ' + urls[i] + ' ' + path.join(args.dirPath, plugins[i]))

def check_plugins():
    '''
    Check plugins in a download directory for differences between the
    remote and local versions.
    '''
    local = set(os.listdir(args.dirPath))
    plugins, urls = get_plugins()

    plugins = set(plugins)
    urls = set(urls)

    normalprintout('************************************\n', GREEN)
    normalprintout('TOTAL = ' + str(len(plugins)) + '\n', RED)
    normalprintout('************************************\n', GREEN)
    normalprintout('************************************\n', BLUE)
    normalprintout('PLUGINS IN POOL, NOT LOCAL:\n', BLUE)

    for plugin in plugins.difference(local):
        normalprintout(plugin + '\n', BLUE)

    normalprintout('************************************\n', BLUE)
    normalprintout('************************************\n', MAGENTA)
    normalprintout('PLUGINS LOCAL, NOT IN POOL:\n', MAGENTA)

    for plugin in local.difference(plugins):
        if (plugin[0] != '.' and plugin != 'README'
        and not plugin.endswith('.py') and not plugin.endswith('.txt')
        and not plugin.startswith('.git')):
            normalprintout(plugin + '\n', MAGENTA)

    normalprintout('************************************\n', MAGENTA)
    normalprintout('************************************\n', YELLOW)
    normalprintout('PLUGINS THAT DIFFER FROM REPOSITORY:\n', YELLOW)

    for plugin in local.intersection(plugins):
        data = os.popen('cd ' + path.join(args.dirPath, plugin) + '; git diff; cd ..').read()
        if len(data) != 0:
            normalprintout(plugin + '\n', YELLOW)
        os.system('cd ..')
    normalprintout('************************************\n', YELLOW)

if __name__ == '__main__':

    parser = argparse.ArgumentParser('Download or check plugins from the public PluMA plugin pool.')

    parser.add_argument('action', metavar='ACTION', action='store', help="Action to take. [ 'download' or 'check' ]")
    parser.add_argument('--directory', dest='dirPath', metavar='DIRECTORY', action='store',
        default=path.join(path.realpath(path.dirname(__file__)), 'plugins'),
        help='Directory to download plugins to or directory where plugins are downloaded to.')

    args = parser.parse_args(args=None if sys.argv[1:] else ['--help'])

    if args.action == 'check':
        check_plugins()
    elif args.action == 'download':
        download_plugins()
    else:
        exit(1)
