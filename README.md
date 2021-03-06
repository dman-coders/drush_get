# drush get-*

This set of utilities is designed to assist quick site copy setups
 with sensible defaults.

There are two main parts to the utility.
* **get site aliases**  - by looking up site-aliases from a central index of
  all sites.
* **get sites** - by stringing together existing drush commands for site cloning,
  together with some local hosting default settings for autoconfiguration.

Both parts are configured and the configurations are inspected using
the `get-setup` command.

### get site aliases

The first utility operates as a dynamic index of site-aliases, by designating
a master site instance (eg, a central dev server) that
we can query for info about other sites and site-aliases
that it knows about, but we haven't heard about before.

By designating an aegir hostmaster instance as our master,
we can now find out all we need to know about all the sites it hosts.

### get sites

The second utility makes a number of inspections on your (local) hosting
environment, and guesses where you want to store site mirrors and how you
want to name local database copies.
It then runs drush rsync, site-install, and sql-sync commands that *should*
produce local working site clones pretty quick.

Requirements
=======

Drush
-----
Drush 8+ is required. In order to run the 'runserver' process at the end, [it's currently neccessary](https://github.com/drush-ops/drush/issues/2090#issuecomment-232172907) to use the 'composer' method of installation, not [the 'phar' download method currently documented](http://docs.drush.org/en/master/install/). Using composer will also ensure that the appropriate version of drush for your available php version will be used.

Drush site-aliases
------------------
This utility extends the usage of [drush remote site-aliases](https://www.drupal.org/node/1401522). You should be familiar with those, and have already established a working site-alias record for your target server. Your user account must have the appropriate ssh key installed on the target server, and the network connection should be able to invoke a 'drush status' command over there.
These requirements are *tested* during the `get-setup` phase, but as there are a huge number of ways you may have this configured (you may want bastions, tunnels, shared user accounts etc), there is no single HOWTO for getting that connected.


Install
=======

* Download this drush command into any of the [supported locations for drush command files](http://docs.drush.org/en/master/commands/). EG:

  ````
  git clone https://github.com/dman-coders/drush_get.git ~/.drush/drush_get
  ````
  
  Or, for a development copy:
  
  ````
  git clone git@github.com:dman-coders/drush_get.git ~/.drush/drush_get
  ````
* Clear the drush cache: 
  
  ```` 
  drush cc drush
  ````

Commands
========

Review and test local configurations

    get-setup

Retrieve site-alias info from a central server.

    get-whois @sitename

Copy a named remote site to your local environment.

    get-site @sitename

Settings
========

For this to work as expected, it helps for you to set defaults up
 for your local development environment to suit your working style beforehand.
Default settings should be added to your ~/.drushrc.php file.
Your current settings can be checked using the get-setup command,
  which should provide prompts and instructions where needed.

You'll need to define
* where to search for unknown site alias definitions (the master)
* where to copy site files down to (your docroots)
* and how to add to your local development databases (an admin SQL user)

The admin db-su, db-su-pw definitions are those documented for
 `drush site-install`
If you have followed those instructions, you are already configured.

Example settings
----------------

The following configs are expected to be put in your
`~/.drushrc.php` or equivalent.

````
    # Where to query for site-aliases I've not heard of before.
    $options['master-alias'] = '@hostmaster.centraldev';

    # Where my local site-alias files are stored
    # @see example.drushrc.php
    $options['alias-path'][] = '/var/drush/site-aliases';

    # Where I build sites on my local machine.
    $options['get-docroot-pattern'] = '/var/www/%short-name/%docroot-name';

    # How to name the locally created site instances.
    $options['get-uri-pattern'] = '%instance.%short-name.localhost';

    # When provisioning a db (if not using Aegir or DevDesktop)
    # what pattern to use.
    # NOTE: in some environments there is a significant difference between 'localhost' and '127.0.0.1'
    $options['get-db-pattern'] = 'mysql://%short-name:%random@127.0.0.1:3306/%short-name_%instance';

    # For drush site-install and drush get.
    # DB connection settings appropriate for administering local MySQL.
    # Should have 'create database' permissions etc.
    $options['db-su'] = 'root';
    $options['db-su-pw'] = 'mypass';
````

### get-master-alias

Identify a (probably remote) Drupal instance that has access to a shared
site-alias repository.
Use this in a shared development workflow, 
so all staff can easily pull in info about all projects as needed.

### alias-path

Where local copies of site-alias `*.alias.drushrc.php` files may be created.
If using the optional --save write-back option,
this alias-path should be writable.
Local copies of the retrieved site-alias info may then be saved there
for quicker future access. If you have more than one directory as a search path,
the first one will be used as the write-back location.

### get-docroot-pattern

Projects will be downloaded and built under here.
This defines my naming convention for building projects.
A simple static pattern may be

    $options['get-docroot-pattern'] = '/var/www/%uri/docroot';

### Tokens in the `get-docroot-pattern`

  Any values in the sites config array may be used, eg
 `%client_name` (an Aegir thing it injects into the `$alias` array)
  Additionally, some extra tokens are made available.
  
  `%short-name` will be the first one of :
  - the 'short-name' value in the site-alias if set.
  - the 'name' from site-alias if set.
  - the 'uri' from the site alias.
  
  `%docroot-name` will be the first one of :
  - the 'instance' from site-alias if set, eg 'dev', 'test', 'uat'.
  - the string 'docroot'

### get-db-pattern

  When provisioning a db (if not using Aegir or DevDesktop manually)
  what pattern to use.
  NOTE: in some environments there is a significant difference between 'localhost' and '127.0.0.1'

    $options['get-db-pattern'] = 'mysql://%short-name:%random@127.0.0.1:3306/%short-name';

   This pattern is resolved and then used by the drush site-install command as
   `--db-url` option, so see `drush help site-install` for troubleshooting.
   
### `db-su`, `db-su-pw`

  As used in drush site-install, the Database superuser credentials
  required for creating new databases.
 

## Preparing the source

Life is no fun if you have not installed drush remotely first.

sql-sync specifically has difficulty if your local and remote versions
are too different from each other.

Modern versions of drush require composer and a high amount of access (not to mention PHP versions)
to work, and that can be prohibitive on legacy sites you are hoping to rescue.
 
[Installing Older Versions of Drush on Shared Hosting Accounts](https://www.drupal.org/node/1181480)
provides some useful instructions. As suggested there, 
[drush 6.1.0](https://github.com/drush-ops/drush/archive/6.1.0.tar.gz) as a stand-alone zip
may be the best bet if you have PHP 5.3.3+.
Below that, the best you can do is [drush5](https://github.com/drush-ops/drush/archive/5.x.tar.gz) (which requires a download of [PEAR ConsoleTable])(http://pear.php.net/package/Console_Table) also. This may require a manual download [Console_table-1.1.3](http://download.pear.php.net/package/Console_Table-1.1.3.tgz)
 
### Source site-alias

The source site MUST be defined by a good site-alias.
The current user must be able to connect directly to the source site
and perform normal drush commands upon it.

A working source site-alias file will probably have a
 `remote-host` and `remote-user` key,
 in addition to the usual `uri` and `root` settings.

````
$aliases['live.site.com'] = array (
  'root' => '/var/www',
  'uri' => 'live.site.com',
  'remote-host' => '123.222.123.222',
  'remote-user' => 'deployuser',
);
````

*Ensure your current user can at least run
````
     drush @live.site.com status
````
Before continuing.


### Exclude source site files

If you are preparing to copy from a source site, one of the
first tasks that will run will be a full file copy (rsync)
from the source to the local copy.
This is done with no assumption of version control and is a flat file copy,
intended to produce a real 'mirror' of the source site.

This can be a slow process if the source site contains a large number of
unexpected files, such as those found in legacy or non-drupal 'static'
directories.

Instead of offering additional filtering options through the get-site action,
it's advised that you *tune your source site-alias first*
- to avoid rsyncing redundant directories.
Normal `drush rsync` rules apply,
and are configured using the supported `drush rsync` options..

This looks like this in a source-sites site-alias file.

````
$aliases['live.site.com'] = array (
  ...
  'command-specific' => array(
    # These directories are legacy, and full of PDFs, ignore them.
    'rsync' => array('exclude-paths' => 'Documents,static,_archive'),
  ),

);
````

For convenience, the *known* drupal site user files like `sites/default/files`
will be excluded in the first download.
Known Drupal user files

## Running the mirror site

After a successful get-site, You have the files and the database
ready to rock. All that's needed is the webserver.
It's usually NOT neccessary to have a running webserver if all you
wanted the copy for was a snapshot or backup to do migrations from,
but it's helpful to prove to yourself that things worked.

You can choose to:
* Set up the new platform and vhost yourself with Apache
* Register it with Apache Dev Desktop or Aegir - if you use those.
* Use the drush built-in mini web server, just for a test.

#### runserver is the simplest

I found a few quirks when using `drush run-server`, The ADD PHP,
 and an old D6 site.
*  I Needed to use Drupal6, not higher.
*  The php-cgi path needed to be defined explicitly.
*  The MySQL had to be connecting on 127.0.0.1, not localhost.

````
drush6 \
  --root=/var/www/beehive/dev \
  --uri=dev.beehive.local \
  --php-cgi=/Applications/DevDesktop/php5_5/bin/php-cgi \
  runserver
````
