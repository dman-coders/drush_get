This set of utilities is designed to assist quick site copy setups with sensible defaults.

This operates as an index of site-aliases, by designating
a master site instance (eg, a central dev server) that
we can query for info about other sites and site-aliases
that it knows about, but we haven't heard about before.

By designating an aegir hostmaster instance as our master,
we can now find out all we need to know about all the sites it hosts.

Commands
========

Review and test local configurations

    get-setup

Retrieve site-alias info from a central server.

    get-whois @sitename

Copy a named remote site to your local environment.

    get @sitename

Settings
========

For this to work as expected, it helps for you to set defaults up
 for your local development environment to suit your working style beforehand.
Default settings should be added to your ~/.drushrc.php file.
Your current settings can be checked using the get-setup command,
  which should provide prompts and instructions where needed.

You'll need to define WHERE to copy site files down to (your docroots)
and how to add to your local development databases (an admin SQL user)

Example settings
----------------

    # Where to query for site-aliases I've not heard of before.
    $options['get-master-alias'] = '@hostmaster.centraldev';

    # Where my local site-alias files are stored
    # @see example.drushrc.php
    $options['alias-path'][] = '/var/drush/site-aliases';


    # Where I build sites on my local machine.
    $options['get-docroot-pattern'] = '/var/www/%short-name/%docroot-name';

### get-master-alias

Identify a (probably remote) Drupal instance that has access to a shared
site-alias repository.
Use this in a shared development workflow, 
so all staff can easily pull in info about all projects as needed.

### alias-path

If using the --save write-back option,
the primary alias-path should be writable, but that is optional.

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
 
