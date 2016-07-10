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
    $options['get-master-alias'] = '@hostmaster.centraldev';

    # Where my local site-alias files are stored
    # @see example.drushrc.php
    $options['alias-path'][] = '/var/drush/site-aliases';

    # Where I build sites on my local machine.
    $options['get-docroot-pattern'] = '/var/www/%short-name/%docroot-name';

    # For drush site-install and drush get.
    # DB connection settings appropriate for administering local MySQL.
    $options['db-su'] = 'drupaladmin';
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
  - the 'role' from site-alias if set, eg 'dev', 'test', 'uat'.
  - the string 'docroot'
   
   
### `db-su`, `db-su-pw`

  As used in drush site-install, the Database superuser credentials
  required for creating new databases.
 

## Preparing the source

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

