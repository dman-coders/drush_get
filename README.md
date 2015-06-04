This set of utilities is designed to assist quick site copy setups with sensible defaults.

Commands
========

 get-setup              Review and test local configurations
 get-whois @sitename    Retrieve site-alias info from a central server.
 get @sitename          Copy a named remote site to your local environment.

Settings
========

For this to work as expected, it helps for you to set those defaults up
 to suit your working style beforehand.
Default settings should be added to your ~/.drushrc.php file.
Your current settings can be checked using the get-setup command,
  which should provide prompts and instructions where needed.

Example settings
----------------

    # Where to query for site-aliases I've not heard of before.
    $options['get-master-alias'] = '@hostmaster.centraldev';
    
    # Where my local site-alias files are stored
    # @see example.drushrc.php
    # If using the --save write-back option,
    # this should be writable, but that is optional.
    $options['alias-path'][] = '/var/drush/site-aliases';

    # Where I build sites on my local machine.
    # Projects will be downloaded and built under here.
    # this defines my naming convention for building projects.
    $options['get-docroot-pattern'] = '/var/www/%short-name/%docroot-name';
    # A simpler alternative may be
    # $options['get-docroot-pattern'] = '/var/www/%uri/docroot';
    
Available Tokens in the get-docroot-pattern
  Any values in the sites config array may be used, eg
  %client_name (an Aegir thing it injects into the $alias array)
  
  %short-name will be the first one of :
  - the 'short-name' value in the site-alias if set.
  - the 'name' from site-alias if set.
  - the 'uri' from the site alias.
  
  %docroot-name will be the first one of :
  - the 'role' from site-alias if set, eg 'dev', 'test', 'uat'.
  - the string 'docroot'
   