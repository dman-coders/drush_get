<?php

/**
 * @file
 * Utility for quick mirroring of sites to local dev environments.
 *
 * Wrapper around drush site-install, drush sql-sync and drush rsync type
 * processes, but built to prefill a load of defaults wherever possible.
 *
 * Designed for teams working on many disparite sites at once to collaborate
 * quicker and use local dev environments (eg acquia dev desktop) more
 * efficiently..
 *
 * Tested against Drush 8, but attempts to be compatible with Drush 6 enough
 * to be able to talk to old sites.
 *
 * Not sure about the significance of '%' VS '@' used in the drush_log
 * $strings replacements. They were used interchangably depending on the year.
 *
 *
 */

/**
 * Implements hook_drush_command().
 */
function get_drush_command() {
  $items = array();

  $options = array(
    'master-alias' => array(
      'description' => dt('Site alias for connecting to a central site instance that knows all about the sites we want to work with.'),
      'example-value' => '@central.dev.server',
    ),
    'local-webroot' => array(
      'description' => dt('Where site copies will be downloaded to and run from.'),
      'example-value' => '/var/www',
    ),
  );

  $items['get-setup'] = array(
    'description' => dt('Review the current configs for "getting" a site and fill in any gaps.'),
    'options' => $options,
    'examples' => array(
      'drush get-setup --master-alias=@central.dev.server' => dt('Connect to central.dev.server using site-alias credentials already set up, and auto-configure from that.'),
      'drush get-setup --remote-host=central.dev.server --remote-user=mylogin' => dt('Connect to central.dev.server using an ssh-keyed account and self-configure.'),
    ),
    'topics' => array('docs-get'),
    'outputformat' => array(
      'default' => 'key-value',
      'pipe-format' => 'string',
      'label' => 'Setup options for "get"',
      'require-engine-capability' => array('format-single'),
    ),

    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['get-whois'] = array(
    'description' => dt('Fetch the named site-alias, querying the master-alias instance if neccessary.'),
    'arguments' => array(
      'site-alias' => 'The site-alias to look for.',
    ),
    'options' => array(
      'master-alias' => $options['master-alias'],
      'refresh' => array(
        'description' => dt('Bool. Force a remote lookup even if a local copy of the site-alias is known. Use this togheter with --save to overwrite an existing site-alias file.'),
      ),
      'save' => array(
        'description' => dt('Writes back the retrieved site-alias record into your local alias-path folder.'),
      ),
      'alias-path' => array(
        'description' => dt('Where to save retrieved sitealias records. If not set, will use the first configured "alias-path" found in your .drushrc.php file, or "~/.drush" if not defined.'),
      ),
    ),
    // Duplicate the site-alias output settings if I can.
    'outputformat' => array(
      'default' => 'config',
      'pipe-format' => 'var_export',
      'variable-name' => 'aliases',
      'hide-empty-fields' => TRUE,
      'private-fields' => 'password',
      'field-labels' => array(
        '#name' => 'Name',
        'root' => 'Root',
        'uri' => 'URI',
        'remote-host' => 'Host',
        'remote-user' => 'User',
        'remote-port' => 'Port',
        'os' => 'OS',
        'ssh-options' => 'SSH options',
        'php' => 'PHP'
      ),
      'fields-default' => array(
        '#name',
        'root',
        'uri',
        'remote-host',
        'remote-user'
      ),
      'field-mappings' => array('name' => '#name'),
      'output-data-type' => 'format-table',
    ),
    'required-arguments' => 1,
    'examples' => array(
      'drush get-whois @mysite.multisitehost.org' => 'Returns the site-alias for the named site (by performing a remote lookup request if needed).',
      'drush get-whois @mysite.multisitehost.org --master-alias=@hostmaster.multisitehost.org --save' => 'Requests the site-alias info from the named hostmaster site, and saves a local copy to your site-aliases folder.',
    ),
    'topics' => array('docs-get'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['get'] = array(
    'callback' => 'drush_get_site',
    'aliases' => array('get-site'),
    'description' => dt('Take a local copy of the named site.'),
    'arguments' => array(
      'source-site-alias' => 'Original site alias to be copied. If the site-alias is not yet known, the --master-alias instance will be queried to try and find it.',
      'example-value' => '@dev.projectname.sharedserver',
    ),
    'options' => $options + array(
        'make-url' => array(
          'description' => dt('The location of a makefile that defines this project. drush-make will be run to build this project to set up the platform. If not provided, the source site may be inspected to deduce this.'),
          'example-value' => 'http://cgit.drupalcode.org/projectname/tree/projectname.make',
        ),
        'git-url' => array(
          'description' => dt('The git URL of the project. Code will be copied from there to set up the site platform. If not provided, the source site may be inspected to deduce this.'),
          'example-value' => 'git@git.company.com:projectname/website-d7.git',
        ),
        'project-name' => array(
          'description' => dt('Project name to use when getting. Used for naming the project folder (/var/www/{projectname}/mirror) and local site (http://mirror.{projectname}.local/).'),
          'example-value' => 'myproject',
        ),
        'instance' => array(
          'example-value' => 'devcopy',
          'description' => dt('Instance name to use when getting. Used for naming the project folder (/var/www/projectname/{instance}) and local site (http://{instance}.projectname.local/) and database. Defaults to "mirror" or is influenced by the patterns.'),

        ),
      ),
    'examples' => array(
      'drush get @projectname.uat.preview' => dt('Copy the named site to your local dev environment. Most local parameters will be defaulted.'),
    ),
    'topics' => array('docs-get'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['docs-get'] = array(
    'description' => 'The "get" commands for quick site copies',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'callback' => 'drush_print_file',
    'callback arguments' => array(dirname(__FILE__) . '/README.md'),
  );
  return $items;
}

/**
 * Execute `drush get-setup` command.
 *
 * Self-test and report.
 * Some checks (self-introspection) will be reporting 'ok' (looks good).
 * Other checks (actually probing if it works) Will be returning 'success'.
 *
 * Ensures:
 * - your local paths are appropriate
 * - your connection to your local db is configured
 * - the expected master-alias site is available.
 */
function drush_get_setup() {
  drush_log(dt("About to review your local setup. This will probe your local storage path for projects, your access to your own database, connections to your remote list of projects, and the status there.", array()), 'info');
  drush_log(dt("These patterns can be adjusted in your own drushrc.php, see 'drush topic get' for more.", array()), 'ok');
  drush_log(dt("#### drush-get patterns ####"));

  $out = array();
  $master_alias = drush_get_option('master-alias', '');
  $strings['@master-alias'] = $master_alias;
  $out['master-alias'] = $master_alias;

  // Check local environment preferences.
  $sampleproject_spec = array(
    'uri' => 'sampleproject.remotehost.demo',
    'name' => 'sampleproject',
    'instance' => 'mirror',
  );
  $get_docroot_pattern = drush_get_option('get-docroot-pattern', '/var/www/%short-name/%docroot-name');
  $local_root = drush_get_process_tokens($get_docroot_pattern, $sampleproject_spec);
  $strings['@get-docroot-pattern'] = $get_docroot_pattern;
  $strings['@local_root'] = $local_root;

  $get_uri_pattern = drush_get_option('get-uri-pattern', '%instance.%short-name.localhost');
  $local_uri = drush_get_process_tokens($get_uri_pattern, $sampleproject_spec);
  $strings['@get-uri-pattern'] = $get_uri_pattern;
  $strings['@local_uri'] = $local_uri;

  drush_log(dt("Your generic 'get-docroot-pattern' is '@get-docroot-pattern' .", $strings), 'ok');
  drush_log(dt("Your generic 'get-uri-pattern' is '@get-uri-pattern' .", $strings), 'ok');
  drush_log(dt("Locally copied sites will be saved in paths such as \n  '@local_root' \nand URLS such as\n '@local_uri' .", $strings), 'ok');

  // Admin database-superuser login is needed.
  $db_su = drush_get_option('db-su');
  $strings['@db-su'] = $db_su;
  if (!$db_su) {
    drush_log(dt("Unable to determine your database super-user. You must set the drush 'db-su' and 'db-su-pw' parameters in your drushrc.php or equivalent. See help for site-install .", $strings), 'error');
    return FALSE;
  }

  $local_db = db_pattern_from_site_spec($sampleproject_spec);
  $strings['@local_db'] = $local_db;
  $strings['@get-db-pattern'] = drush_get_option('get-db-pattern', 'mysql://%short-name_%instance:%random@127.0.0.1:3306/%short-name_%instance');;
  drush_log(dt("Your generic database connection pattern 'get-db-pattern' is '@get-db-pattern' and your database admin user is '@db-su' .", $strings), 'ok');
  drush_log(dt("Locally copied sites will be stored in the database identified like\n  '@local_db' .", $strings), 'ok');

  drush_log(dt("These patterns can be adjusted in your own drushrc.php, see 'drush topic get' for more.", $strings), 'ok');

  // Expected configs used and process should be as much like
  // drush_core_site_install()
  // as possible.
  // $db_spec is a d7 style db info array.
  $db_spec = drush_convert_db_from_db_url($local_db);

  drush_log(dt("Testing to see if we have permissions to make a database using your configured db-su:db-su-pass (%db-su) credentials .", $strings), 'info');
  /**
   * @var $sql \Drush\Sql\SqlBase
   */
  $su_sql = drush_sql_get_class($db_spec);
  try {
    $su_sql->drop_or_create();
  } catch (Error $e) {
    return drush_set_error(dt('Failed to create database: @error', array('@error' => $e->getMessage())));
  }
  drush_log(dt('Looks like we had the ability to make databases.'), 'success');

  drush_log(dt("#### drush-get master ####", $strings), 'info');
  // Test if we can use the new credentials to create a table;
  // We are re-establishing a NEW $sql, as the previous $su_sql had its
  // privileges elevated.
  // The test is to see if delegating to the project-specific user worked.
  // Try that now.
  $sql = drush_sql_get_class($db_spec);
  $success = FALSE;
  try {
    $sql->query(sprintf('DROP TABLE IF EXISTS %s;', 'deleteme'));
    $success = $sql->query(sprintf('CREATE TABLE %s (id INT);', 'deleteme'));
  } catch (Error $e) {
    return drush_set_error(dt('Failed to create table in database: @error', array('@error' => $e->getMessage())));
  }

  if ($success) {
    drush_log(dt("Looked like we could add a database and grant permissions on it and work on it.", $strings), 'success');
    $out['db-su-access'] = TRUE;
  }
  else {
    drush_log(dt("Looked like we failed to connect, create db, or grant permissions on a database as admin user '@db-su' trying to connect to, create and modify @local_db .", $strings), 'error');
    drush_log(dt("Check the values of 'db-su', 'db-su-pw' and 'get-db-pattern' in your drushrc.php or equivalent.", $strings), 'error');
    return drush_set_error(dt('Failed to gain admin access to local database.', $strings));
  }
  // Clean up.
  drush_log(dt("Removing test database @local_db .", $strings), 'debug');
  // Unimplimented? $su_sql->delete();
  $su_sql->query(sprintf('DROP DATABASE IF EXISTS %s;', $db_spec['database']));

  drush_log(dt("Local Database info and access seems fine.", $strings), 'success');

  drush_log(dt("Now checking master-alias @master-alias for upstream info about sites.", $strings), 'notice');
  if (empty($master_alias)) {
    drush_log("No master-alias is set", 'warning');
    drush_print("You should define your default master-alias option in your drushrc.php");
    drush_print("The master-alias identifies a site on your central server that contains a list of all known site-aliases.");
    drush_print("@hostmaster.aegir.devserver would be good. It must be a full bootstrapped site, not just a server login.");
    drush_print("See 'drush docs-get' topic for more.");
  }
  else {
    drush_log(dt("master-alias is @master-alias", $strings), 'ok');
    $out['master-alias'] = $master_alias;
    // Test the listed master-alias is known.
    $master_spec = drush_sitealias_get_record($master_alias);
    if (empty($master_spec)) {
      drush_log(dt("Could not find the master-alias named '@master-alias'", $strings), 'error');
      drush_print(dt("Ensure this ID is correct and findable in your known site-aliases locations.", $strings));
      drush_print("See 'drush docs-get' topic for more.");
    }
    else {
      $strings['@remote-host'] = $master_spec['remote-host'];
      $strings['@remote-user'] = $master_spec['remote-user'];
      drush_log(dt("master-alias is to be found at @remote-user@@remote-host", $strings), 'ok');

      // Test connection to master-alias.
      drush_log(dt('Checking connection to remote-host:"@remote-host" and the drush version there.', $strings), 'info');

      $commandline_options = array();
      // I wanted to avoid a timeout here, as this is just a probe,
      // but these options get interpreted by
      // drush and therefore die when --strict options are on.
      // $commandline_options = array(
      // 'ssh-options' => '-o ConnectTimeout=10'
      // );
      //
      // Before testing the master site itself, test the remote drush version.
      // It seems we get best results when using drush6+ ! Drush4 bad.
      // Use --strict=0 because drush 'version' does not expect
      // the 'uri' param that drush_invoke_process adds.
      $lookup_results = drush_invoke_process($master_alias, 'version --pipe --strict=0', $commandline_options, array(), FALSE);
      if ($lookup_results['error_status'] != 0) {
        drush_log(dt('Connection to "@remote-host" to check the remote drush version failed. Try this command with --verbose to see what may have gone wrong.', $strings), 'error');
        return FALSE;
      }
      // Version result may come back as 6.2, or 5.0-dev.
      // Just want the major really.
      $remote_drush_version = floatval($lookup_results['output']);
      $strings['@remote_drush_version'] = $lookup_results['output'];
      drush_log(dt('Remote connection succeeded and drush version on "@remote-host" was reported as @remote_drush_version.', $strings), 'success');

      // Boring. The way data was returned is different between drush 5 & 6.
      $drush_bootstrap_key = 'bootstrap';
      if ($remote_drush_version < 6) {
        drush_log(dt('Some features of the drush_get tool have only been found to work against drush 6+ . Lower versions MAY work, but the diagnostics we have to work with are more likely to fail. I suggest installing a higher version of drush, and checking the drush-script for the target server.', $strings), 'warning');
        $drush_bootstrap_key = 'Drupal bootstrap';
      }

      drush_log(dt('Checking availability and status of master-alias site @master-alias', $strings), 'info');
      $lookup_results = drush_invoke_process($master_alias, 'status', $commandline_options, array(), FALSE);
      // $lookup_results['output'] = "raw text"
      // $lookup_results['object'] = array() # Array version of that.
      // $lookup_results['error_status'] = 0 # If lucky.
      // Unlucky looks like
      // $lookup_results['log'] = array([messages]);
      // $lookup_results['error_log']
      // = array(DRUPAL_USER_LOGIN_FAILED
      // =>  "Could not login with user account `1'.");
      // .
      $bootstrap = $lookup_results['object'][$drush_bootstrap_key];

      if ($bootstrap == 'Successful') {
        drush_log(dt('Connection to master-alias @master-alias and Drupal bootstrap and status check succeeded.', $strings), 'success');
        // print_r($lookup_results['object']);.
        if ($remote_drush_version >= 6) {
          $strings['@remote_alias_count'] = count($lookup_results['object']['drush-alias-files']);
          drush_log(dt('Remote master-alias knows about @remote_alias_count alias files over there.', $strings), 'info');
        }
      }
      else {
        drush_log(dt('Connection to master-alias @master-alias failed. Try this command with --verbose to see what may have gone wrong.', $strings), 'error');
        print_r($lookup_results);
        print_r($lookup_results['error_log']);
      }

    }
  }
  $alias_path = drush_get_alias_path();
  if (!empty($alias_path)) {
    $strings['%alias_path'] = $alias_path;
    drush_log(dt('When caching info about site-aliases, they will be saved in %alias_path . This is the first alias-path value listed in your drushrc.php settings.', $strings), 'ok');
    $out['alias_path'] = $alias_path;
  }
  else {
    drush_log(dt("I don't have a place to store local cached copies of site-aliases retrieved from the master index. This would be the first alias-path value listed in your drushrc.php settings.", $strings), 'warning');
  }

  return $out;
}

/**
 * Calculate the appropriate tokenized strings for a local project.
 *
 * Used to generate uris, paths, identifiers and things.
 * See the README.md topic on available tokens.
 *
 * @param string $pattern
 *   Pattern for deriving the project path using substitutions.
 * @param array $site_spec
 *   A site alias specification
 *   containing a number of keys that could be useful as substititions.
 *
 * @return string
 *   The processed result.
 */
function drush_get_process_tokens($pattern, $site_spec) {
  $strings = array();
  // Copy all strings and paths as substitute tokens.
  foreach ($site_spec as $key => $value) {
    if (is_string($value)) {
      $strings['%' . $key] = $value;
    }
  }
  $strings += @(array) $site_spec['path-aliases'];

  if (!isset($strings['%short-name'])) {
    if (isset($strings['%name'])) {
      $strings['%short-name'] = $strings['%name'];
    }
    else {
      $strings['%short-name'] = $strings['@local_uri'];
    }
  }
  // short-name must always be safe for filesystem and database identifiers.
  $strings['%short-name'] = preg_replace('/[^a-z0-9]+/', '_', strtolower($strings['%short-name']));

  // safe-uri can be used for longer slugs
  // - Good for a database name if shortname is reserved.
  if (isset($strings['@local_uri'])) {
    $strings['%safe-uri'] = preg_replace('/[^a-z0-9]+/', '_', strtolower($strings['@local_uri']));
  }

  if (!isset($strings['%docroot-name'])) {
    if (isset($strings['%instance'])) {
      $strings['%docroot-name'] = $strings['%instance'];
    }
    else {
      $strings['%docroot-name'] = 'docroot';
    }
  }

  $strings['%random'] = _drush_get_random();
  return strtr($pattern, $strings);
}

/**
 * Generate a random alphanumeric string (as for a password).
 *
 * This is a copy of Drupal core's user_password() function.
 * as duplicated by provision.inc also.
 *
 * @param int $length
 *   Length of random string.
 *
 * @return string
 *   Random string.
 *
 * @see user_password()
 */
function _drush_get_random($length = 10) {
  $allowable_characters = 'abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789';
  $len = strlen($allowable_characters) - 1;
  $pass = '';
  for ($i = 0; $i < $length; $i++) {
    $pass .= $allowable_characters[mt_rand(0, $len)];
  }
  return $pass;
}

/**
 * Execute `drush get-whois` command.
 *
 * If the named site alias is unknown, query the master-alias site for it.
 *
 * Optionally store the info locally, if a local site-alias file
 * does not already exist..
 *
 * @param string $site_alias
 *   Site identifier. eg @www.example.com.
 *
 * @return array
 *   site-alias specification array. Similar format to drush site-alias command.
 */
function drush_get_whois($site_alias) {
  // nb.
  // _alias is the string identifier,
  // _spec is the array that it represents.
  // _string is the text version of that array that the remote call may give us.
  $strings = array(
    '@site-alias' => $site_alias,
  );
  $site_spec = NULL;
  $site_name = ltrim($site_alias, '@');

  if (!drush_get_option('refresh', FALSE)) {
    // Check the local list of known aliases first.
    $site_spec = drush_sitealias_get_record($site_alias);
    if (!empty($site_spec)) {
      drush_log(dt('Local record of @site-alias found.', $strings), 'ok');
    }
    else {
      drush_log(dt('No local record of @site-alias found. Proceeding with upstream lookup.', $strings), 'info');
    }
  }

  if (empty($site_spec)) {
    $master_alias = drush_get_option('master-alias', '');
    $strings['@master-alias'] = $master_alias;
    $master_spec = drush_sitealias_get_record($master_alias);
    if (!empty($master_spec)) {
      // Query the master-alias for this info.
      drush_log(dt('Asking master-alias @master-alias if it knows about @site-alias', $strings), 'info');
      $lookup_results = drush_invoke_process($master_alias, 'site-alias', array($site_alias), array('format' => 'config'), FALSE);
      // This returns the site_alias snippet in the $lookup_results['output'];
      // Notably, it does not return an array of data? Just a string for me.
      // Output is empty string on failure.
      if (!empty($lookup_results['output'])) {
        drush_log(dt('Retrieved site spec of @site-alias from @master-alias ok.', $strings), 'info');
        $site_spec_string = $lookup_results['output'];
        // I gotta eval it to get the data out again.
        $aliases = array();
        eval($site_spec_string);
        $site_spec = $aliases[$site_name];
      }
    }
    else {
      drush_log(dt('There is no master-alias configured to query for further site-alias indexes. Cannot look up any further. See "drush topic get" for instructions on setting this up. ', $strings), 'warning');
    }
  }

  if (empty($site_spec_string)) {
    drush_log(dt('Could not find a site-alias entry for @site-alias', $strings), 'error');
    return NULL;
  }

  // If we fetched a remote site description, we may also need to make extra
  // notes about how to connect to its ssh server.
  if (!empty($site_spec['server'])) {
    // Aegir-flavoured.
    if ($site_spec['server'] = '@server_master') {
      // Presumably, the specs used to connect to the $master_alias
      // are the specs we need to use to connect to the things it looks after.
      $site_spec['parent'] = $master_alias;
      // There is a chance that just merging everything from $master_alias
      // into this spec may be messy, but it just-works pretty neatly.
      // TODO revisit now that drush has deprecated partn inheritance.
    }
  }

  // Optionally save that config locally for next time.
  if (drush_get_option('save', '')) {
    // Build the PHP string.
    $site_spec_string = "\$aliases['$site_name'] = " . var_export($site_spec, TRUE) . ';' . PHP_EOL;

    $overwrite = drush_get_option('refresh');
    $saved = drush_get_save_alias($site_alias, $site_spec_string, $overwrite);
    if (!$saved) {
      drush_log(dt('Did not save the site-alias to the expected site-alias directory.', $strings), 'error');
    }
  }
  if ($site_spec) {
    return array($site_name => $site_spec);
  }
  else {
    return NULL;
  }
}

/**
 * Save a given site spec into our default site-aliases folder.
 *
 * @param string $site_alias
 *   Site identifier.
 * @param array|string $site_spec_string
 *   PHP config file snippet.
 * @param bool $overwrite
 *   Whether to overwrite an existing file found there.
 *
 * @return bool|string
 *   The successfully saved filepath. FALSE on failure.
 */
function drush_get_save_alias($site_alias, $site_spec, $overwrite = FALSE) {
  if (empty($site_alias) || empty($site_alias)) {
    drush_log(dt('Empty parameters given to @FUNCTION', array('@FUNCTION' => __FUNCTION__)), 'warning');
    return FALSE;
  }
  // OK, if you gave me a site_spec array, I can deal with that also.
  // Cast it to a config string on the fly here.
  if (is_array($site_spec)) {
    $site_spec_string = '$aliases["' . $site_alias . '"] = ' . var_export($site_spec, TRUE) . ';' . PHP_EOL;
  }
  else {
    $site_spec_string = $site_spec;
  }

  $alias_path = drush_get_alias_path();

  $clean_site_alias = ltrim($site_alias, '@');
  $strings['@site_alias'] = $clean_site_alias;
  $alias_filepath = $alias_path . DIRECTORY_SEPARATOR . $clean_site_alias . '.alias.drushrc.php';
  $strings['@alias-filepath'] = $alias_filepath;

  if (file_exists($alias_filepath) && !$overwrite) {
    drush_log(dt('An alias file for this site already exists at @alias-filepath . Use the --refresh option if you really want to overwrite it.', $strings), 'warning');
    return FALSE;
  }

  // Prepare and save that info locally.
  $file_contents = '<' . '?php' . PHP_EOL . $site_spec_string;
  $success = file_put_contents($alias_filepath, $file_contents);

  if (!$success) {
    drush_log(dt('Could not save the alias info to @alias-filepath', $strings), 'error');
    return FALSE;
  }

  // Paranoia.
  // Assert that the site-alias file code is valid,
  // or at least will not break things.
  // Saving a syntax error into an alias.drushrc file could cripple drush.
  // http://php.net/manual/en/function.php-check-syntax.php
  system('php -l ' . $alias_filepath, $ret);
  if ($ret !== 0) {
    drush_log(dt(__FUNCTION__ . ' Syntax check on the "@site_alias" site-alias config code failed. This is unexpected and indicates a problem with the data supplied from upstream. Run this command with --debug on for clues.', $strings), 'error');
    drush_log(print_r($file_contents, 1), 'error');
    drush_log(dt('Removing the broken @alias-filepath code file for now, as it would cause you problems.', $strings), 'warning');
    unlink($alias_filepath);
    return FALSE;
  }

  drush_log(dt('Saved the alias info to @alias-filepath', $strings), 'ok');
  return $alias_filepath;
}

/**
 * Return the local file directory where we save site-aliases.
 */
function drush_get_alias_path() {
  $alias_path = drush_get_option('alias-path', array());
  if (is_array($alias_path)) {
    $alias_path = reset($alias_path);
  }
  return $alias_path;
}

/**
 * Pulls down a copy of an existing site and sets up a clone locally.
 *
 * A drush command.
 *
 * @return null
 */
function drush_get_site() {
  $strings = array();
  $site_list = func_get_args();
  if (empty($site_list)) {
    drush_log(dt('No site given', $strings), 'warning');
    return NULL;
  }

  foreach ($site_list as $source_site_alias) {
    drush_get_single_site($source_site_alias);
  }
}

/**
 * Performs 'get' on a single site.
 *
 * This function verbosely wraps a series of drush steps.
 * Almost everything we do could be done by a crafted series of drush calls.
 * This just adds the craft and the error handling.
 * Where possible, the steps are performed in separate functions, while
 * this one just ties them all together.
 *
 * @param $source_site_alias
 *
 * @return bool  Success.
 */
function drush_get_single_site($source_site_alias) {
  // Sometimes we override the drush '--yes' flag temporarily.
  $affirmative = drush_get_context('DRUSH_AFFIRMATIVE');

  $strings['@source_site_alias'] = $source_site_alias;
  drush_log(dt("Attempting to fetch @source_site_alias", $strings), 'info');

  $source_site_spec = drush_sitealias_get_record($source_site_alias);
  if (empty($source_site_spec)) {
    drush_log(dt('Could not find a site-alias entry for @source_site_alias . Check the ID, or try "drush get-whois -v @source_site_alias" for more details.', $strings), 'error');
    return FALSE;
  }

  $strings['@source_root'] = $source_site_spec['root'];
  $strings['@remote-host'] = $source_site_spec['remote-host'];
  drush_log(dt('Info retrieved describing @source_site_alias. It is in @source_root on @remote-host .', $strings), 'success');

  if (!($remote_drupal_version = drush_get_remote_status($source_site_alias))) {
    drush_log(dt('Remote site cannot be worked with.', $strings), 'fail');
    return FALSE;
  }

  // So far so good.
  // Now to do :
  // * Local platform setup, using the get_setup preferences.
  // * A file-copy down
  // * Database sync down
  // * Local status check.
  $short_name = drush_get_short_name_from_site_spec($source_site_spec);
  $strings['%short_name'] = $short_name;
  $instance = $strings['%instance'] = drush_get_option('instance', 'mirror');

  // Derive the local dev site definition.
  $local_site_spec = array(
    'name' => $short_name,
    'instance' => $instance,
    'path-aliases' => array(),
  );

  $get_uri_pattern = drush_get_option('get-uri-pattern', '%instance.%short-name.localhost');
  $local_uri = drush_get_process_tokens($get_uri_pattern, $local_site_spec);
  $local_site_spec['uri'] = $local_uri;
  $strings['@local_uri'] = $local_uri;

  $get_docroot_pattern = drush_get_option('get-docroot-pattern', '/var/www/%short-name/%docroot-name');
  $local_root = drush_get_process_tokens($get_docroot_pattern, $local_site_spec);
  $local_site_spec['root'] = $local_root;
  $strings['@local_root'] = $local_root;

  // A path-based local_root_alias is enough to start doing other processes.
  // Wierdness happens if this is slightly off
  // - that slash hash makes a huge difference in whether rsync works!
  $local_site_alias = "$local_root/#$local_uri";
  $local_site_spec['alias'] = $local_site_alias;
  $strings['@local_site_alias'] = $local_site_alias;
  drush_log(dt("Your downloaded project will have the short_name:%short_name.", $strings), 'ok');
  drush_log(dt("Locally copied site '@local_uri' will be deployed into '@local_root' .", $strings), 'ok');

  // If we want to continue emulating multisites locally, set sites-subdir.
  // This is useful to identify assumptions about hard-coded paths etc,
  // and will maintain aegir like setups BUT things remain tricky.
  // If we choose not to do that, and fall back to 'default', many shortcuts
  // remain easier.
  //
  // If using a multisite named sites dir,
  // drush runserver does not cooperate with multisite without help.
  // Current work-arounds are to copy the settings into default
  // (which is not quite right as it fudges it)
  // or edit sites.php to redirect 'default' => $local_uri
  // which is more official.
  #$sites_subdir = $local_site_spec['uri'];
  $sites_subdir = 'default';
  $local_site_spec['path-aliases']['%site'] = "sites/${sites_subdir}";
  $strings['%sites_subdir'] = "sites/${sites_subdir}";

  // Look ahead to see if the local site already exists -
  // This would be the case if we are resetting or re-downloading a previous
  // copy.
  if (get_can_bootstrap($local_site_spec)) {
    $local_site_spec['#bootstrap'] = TRUE;
  }
  // If that came back as bootstrapped,
  // then we have a local working environment already,
  // so can skip the hard site-install, and re-use the db credentials.

  // What will the new db connection be?
  $local_db = db_pattern_from_site_spec($local_site_spec);
  $local_site_spec['db-url'] = $local_db;
  $strings['@local_db'] = $local_db;
  drush_log(dt("Locally copied site will be given database '@local_db' .", $strings), 'ok');

  drush_print(var_export($local_site_spec, 1));
  // $sa_result = drush_invoke_process($local_site_alias, 'site-alias', [$local_site_alias], []);

  if (!drush_confirm('Do the values above look correct?')) {
    drush_log(dt("You may want to exit and try again with the --project-name option set. For other options see drush topic docs-get.", $strings), 'ok');
    return 'Exiting';
  }

  // BEGIN
  // FETCH CODEBASE

  // See if other methods of downloading are available before rsyncing.
  // Look for extra clues in the site-alias spec - like git info.
  // @see https://www.drupal.org/node/1849304
  $downloaded = drush_get_codebase_from_download($source_site_spec, $local_site_spec);

  // Most sites won't have that spec to use.
  if (! $downloaded) {
    if (!drush_get_codebase($source_site_alias, $local_site_alias, $local_root)) {
      drush_log(dt('Codebase download failed.', $strings), 'fail');
      return FALSE;
    }
  }

  // SITE INSTALL
  //
  // BIG WARNING about drupal6 unresolved site-install issues.
  if ($remote_drupal_version < 7) {
    drush_log(dt("With very old Drupal versions, modern site-install may fail.", $strings), 'warning');
    drush_log(dt("If you see errors like 'Call to undefined function db_result()' then it's best you run install.php locally yourself at this point.", $strings), 'warning');
  }

  // Now initialize a site directory in the new platform.
  // The site we install will be redundant, but the process of installing a
  // site is expected to do a lot of housekeeping.
  // It also serves as a smoke-test to ensure that running is possible.
  $do_site_install = TRUE;

  if ($local_site_spec['#bootstrap']) {
    drush_log(dt("It's quite OK to skip this first-time step when replacing an existing working copy, if you are planning to downsync later.", $strings), 'warning');
    if (drush_confirm("Shall we skip the from-scratch re-install?")) {
      $do_site_install = FALSE;
    }
  }

  if ($do_site_install && !drush_get_site_setup($source_site_spec, $local_site_spec)) {
    drush_log(dt('Site install failed.', $strings), 'fail');
    return FALSE;
  }

  // Set up some config hints, so we can work on the site asap.
  drush_get_configure_project_drush($source_site_spec, $local_site_spec);

  if (!get_can_bootstrap($local_site_spec)) {
    // If we can't bootstrap yet, something is wrong and trying to do more
    // is a bad idea. getting user files will fail with an invalid files dir.
    drush_log(dt('Bootstrap of new site failed. Halting now to avoid more problems.', $strings), 'fail');
    drush_set_context('DRUSH_AFFIRMATIVE', FALSE);
    return FALSE;
  }

  // DATABASE
  if (!drush_get_database($source_site_alias, $local_site_alias)) {
    drush_log(dt('Database sync failed.', $strings), 'fail');
    return FALSE;
  }
  
  // USER FILES
  if (!drush_get_user_files($source_site_alias, $local_site_alias)) {
    drush_log(dt('File sync failed.', $strings), 'fail');
    return FALSE;
  }

  // Reset the yes/no state so it doesn't mess up other logic.
  drush_set_context('DRUSH_AFFIRMATIVE', $affirmative);

  drush_print(dt("#########"));
  drush_print(dt("You can probably run the site now, by invoking: \n  drush @local_site_alias runserver --uri=@local_uri /", $strings));
  drush_print(dt("#########"));
  if (drush_confirm('Do you want to run the server now?')) {
    // Try to turn of PHP strict warnings as they are the least of our problems
    // if trying to debug 'runserver'. (drush > 6 AFAIK)
    $runserver_success = drush_invoke_process($local_site_alias, 'runserver', ['/'], ['--uri=' . $local_uri, '--variables=error_level=0']);
    $strings['%runserver_success'] = $runserver_success;
    drush_print(dt("Ran stand-alone web server %runserver_success", $strings));
  }

}

function get_can_bootstrap(&$site_spec) {
  $status = drush_invoke_process($site_spec, 'status', array(), array(), FALSE);
  if (!empty($status['object']['bootstrap']) || !empty($status['object']['Database'])) {
    $site_spec['#bootstrap'] = TRUE;
    return  TRUE;
  }
  return FALSE;
}

/**
 * Sanity-check the remote site.
 *
 * @param string $source_site_alias
 *   Should be able to supply a version of the site_spec $site_alias_record
 *   interchangably with the site_alias string.
 *
 * @return bool|float
 *   FALSE if we cannot continue.
 *   Drupal version if bootstrap was found.
 */
function drush_get_remote_status($source_site_alias) {
  $strings['@source_site_alias'] = $source_site_alias;
  drush_log(dt('Attempting to check status of the source site... You should already have the appropriate connection authentication in place to run "drush @source_site_alias status"', $strings), 'info');
  // This call echoes the result to the screen.
  // Setting backend_options FALSE seems to quiet that.
  $backend_options = FALSE;
  $remote_status_request = drush_invoke_process($source_site_alias, 'core-status', array(), array(), $backend_options);
  $remote_status = $remote_status_request['object'];
  // Status response is different on older drush. Deal with anything we can.
  // 'Drupal bootstrap' drush 7
  $remote_bootstrapped = isset($remote_status['bootstrap']) ?
    $remote_status['bootstrap'] : (isset($remote_status['Database']) ?
      $remote_status['Database'] : FALSE);
  if ($remote_bootstrapped) {
    drush_log(dt('Source site database status is connected, so things seem to be working', $strings), 'debug');
  }
  else {
    drush_log(dt('Source site database status is not available, so pulling stuff from there will fail. This problem needs to be fixed before we can continue', $strings), 'error');
    return FALSE;
  }

  $remote_drush_version = isset($remote_status['drush-version']) ?
    $remote_status['drush-version'] : (isset($remote_status['Drush version']) ?
      $remote_status['Drush version'] : FALSE);
  $strings['%remote_drush_version'] = $remote_drush_version;
  drush_log(dt('Remote drush version is %remote_drush_version', $strings), 'debug');
  if (floatval($remote_drush_version) < 6) {
    drush_log(dt("Remote drush version found on @source_site_alias was '%remote_drush_version' .\n THIS USUALLY LEADS TO SQL-SYNC problems when different versions of drush try to communicate. You should really try to to upgrade the remote drush to at least 6+, but if not, then the final sql-sync will fail, and you'll have to do it in your own way. \nFor more instructions, see drush topic docs-get", $strings), 'warning');
    if (!drush_confirm('Do you want to continue anyway?')) {
      return FALSE;
    }
  }

  $remote_drupal_version = isset($remote_status['drupal-version']) ?
    $remote_status['drupal-version'] : (isset($remote_status['Drupal version']) ?
      $remote_status['Drupal version'] : FALSE);
  $strings['%remote_drupal_version'] = $remote_drupal_version;
  drush_log(dt('Remote drupal version is %remote_drupal_version', $strings), 'debug');
  if (floatval($remote_drupal_version) < 7) {
    drush_log(dt("Remote drupal version found on @source_site_alias was '%remote_drupal_version' .\n THIS MAY LEAD TO SITE-INSTALL PROBLEMS when different versions of PHP try to work on it.\n You may have to use a different local version of PHP or drush when installing. You may see: 'Error: Call to undefined function db_result()' or 'The site install task failed' later on. This is related to limited backwards-comapatibility in drush versions.", $strings), 'warning');
    if (!drush_confirm('Do you want to continue anyway?')) {
      return FALSE;
    }
  }

  // Having the local info know the remote files path or other basic info
  // would be useful later.
  // Consider refactoring to hang on to this fetched info.

  return floatval($remote_drupal_version);
}

/**
 * Perform variable-get on the target site.
 *
 * @param $site_alias
 */
function drush_get_remote_var($site_alias, $variable) {
  $remote_status_request = drush_invoke_process($site_alias, 'vget', array($variable), array('--exact'), FALSE);
  // TODO: Error-checking?
  return $remote_status_request['output'];
}

/**
 * Initiate download of the remote real site.
 *
 * (ignore VCS , get the real thing).
 */
function drush_get_codebase($source_site_alias, $local_site_alias, $local_root) {
  $strings = array(
    '@source_site_alias' => $source_site_alias,
    '@local_site_alias' => $local_site_alias,
  );

  drush_log(dt("About to rsync MOST FILES from @source_site_alias to @local_site_alias.\n This is likely to take a while with big sites. This is a raw file copy, mirroring the real site, not using source control, and will exclude content that has already been excluded via the source sites site-alias settings.", $strings), 'info');
  if (!drush_mkdir($local_root, TRUE)) {
    drush_log(dt("Could not prepare local root directory @local_root to copy into. Probably permissions.", $strings), 'error');
    return FALSE;
  }

  $rsync_args = array(
    $source_site_alias,
    $local_site_alias,
  );
  $rsync_options = array(
    // If I define exclude-paths here,
    // it seems to override the site-alias ones.
    // '--exclude-paths=backup_migrate'.
    // Copying around mixed settings files will usually be bad news.
    '--exclude-conf',
    // In order to speed up bootstrapping, exclude files folders during
    // this phase of mirroring.
    '--exclude-files',
    // However, --exclude-files will not exclude files found in OTHER
    // multisite dirs, or may pull down symlinked folders.
    '--exclude-other-sites',
    // exclude-other-sites leaves 'default' behind,
    // leaving us with no default.settings?
    '--include-paths=sites/default'
    // '--exclude-sites'.
  );

  // @TODO --exclude-files did not work in all cases ...
  // I got all (4G) user files downloaded last time.
  // Investigate. (and log better)

  /*
  // Recent version of drush stopped assuming yes when dispatching
  // and defaulted to no??
  // Mean that things get skipped unless we
  // either force a yes,
  // (first Save the current state:)
  $affirmative = drush_get_context('DRUSH_AFFIRMATIVE');
  // drush_set_context(DRUSH_AFFIRMATIVE, TRUE);
  // ..or leave it interactive.
  // HOWEVER, interactive sometimes fails to register what's happening
  // over a tunnel (dunno why)
  */

  // Run the proc.
  $backend_options = array('interactive' => TRUE);
  drush_log(print_r([
    'command' => 'rsync',
    'args' => $rsync_args,
    'options' => $rsync_options,
    'backend' => $backend_options
  ], 1), 'debug');
  // The rsync may throw errors due to a premature attempt at bootstrapping
  // the local site if some broken local version already exists
  // in the target location.
  $rsynced_success = drush_invoke_process('@self', 'rsync', $rsync_args, $rsync_options, $backend_options);

  if (!$rsynced_success || $rsynced_success['error_status']) {
    if ($rsynced_success['error_status']) {
      if (!empty($rsynced_success['error_log'])) {
        foreach ($rsynced_success['error_log'] as $log) {
          drush_log($log, 'error');
        }
      }
      else {
        drush_log('Unknown error returned from rsync attempt', 'error');
      }
    }
    else {
      drush_log(dt("Rsync result was FALSE, yet no error status reported. This may happen when tunneling or other things are happening..", $strings), 'error');
    }
    // If the alleged failure was a false negative, Bowl on regardless.
    drush_log("Rsync returned a complaint, but sometimes that's just due to file permissions or conflicts, (or just no files needed) especially if you are re-downloading to an existing attempt.", 'warning');
    print_r($rsynced_success);
    if (!drush_confirm("Do you want to continue anyway?")) {
      return FALSE;
    }
  }
  drush_log(dt("Completed rsync site files from @source_site_alias to @local_site_alias. We now have a platform to build on.", $strings), 'success');
  return TRUE;
}


/**
 * If the site-alias had download info, use that.
 *
 *   $aliases['my.project.url'] = [
 *     'root' => '/var/www/my.project.url/htdocs/',
 *     'uri' => 'my.project.url',
 *     'download' => [
 *       'type' => 'git',
 *       'url' => 'git@gitlab.com:me/my-project.git',
 *       'revision' => 'master',
 *      ],
 *   ];
 *
 * @param $source_site_spec
 * @param $local_site_spec
 * @return bool
 */
function drush_get_codebase_from_download($source_site_spec, $local_site_spec) {
  if (!empty($source_site_spec['download'])) {
    // We may have enough information to fetch site from a git checkout.
    drush_log(var_export($source_site_spec['download'], 1), 'info');
    $type = $strings['%type'] = $source_site_spec['download']['type'];
    drush_print(dt('Looks like we may be able to fetch the codebase via %type instead of rsync. This has advantages and disadvantages.', $strings));
    if (!drush_confirm(dt('Do you want to try this?'))) {
      return FALSE;
    }
    $success = FALSE;
    switch ($type) {
      case 'git':
        // If first time
        #print_r(get_defined_vars());
        if (!is_dir($local_site_spec['root'])) {
          $command = 'git';
          $options = array(
            'clone',
            $source_site_spec['download']['url'],
            $local_site_spec['root']
          );
          if (!empty($source_site_spec['download']['revision'])) {
            $options[] = '--branch ' . $source_site_spec['download']['revision'];
          }

        }
        else {
          // TODO Check if it's a valid git repo first.
          $command = 'git';
          $options = array(
            '-C ' . $local_site_spec['root'],
            'pull',
          );
        }

        $exec = $command . ' ' . implode(' ', $options);
        drush_log("Running drush command\n" . $exec);
        $success = drush_shell_exec($exec, 2);
        drush_log(print_r(drush_shell_exec_output(), 1), 'debug');
        break;

      default :
        drush_log(dt('Download method "%method" is not yet supported.', $strings), 'warning');
        return FALSE;
    }
    if ($success) {
      drush_log(dt('Successfully fetched codebase from %type download.', $strings), 'success');
    }
    else {
      drush_log(dt('Failed to fetch codebase from %type download.', $strings), 'error');
      drush_print(print_r(drush_shell_exec_output()));
    }
    return $success;
  }
  return FALSE;
}

function drush_get_configure_site_settings($source_site_spec, $local_site_spec) {
  $root = $local_site_spec['root'];
  $site_subdir = $local_site_spec['path-aliases']['%site'];

  // The permissions that drupal site-install does gets in our way.
  if (!is_writable("${root}/${site_subdir}")) {
    chmod("${root}/${site_subdir}", 0775);
  }
  // Warn about the dangers of copying settings.php around.
  // This file should almost always be unique and not in source control.
  $settings_file = "${root}/${site_subdir}/settings.php";
  if (file_exists($settings_file)) {
    // Before a clean site-install, any pre-existing settings.php - if present -
    // is likely to get in the way, either because it contains remote details
    // or is just invalid. Or already has its permissions locked.
    // Deleting it is the easiest way to give site-install a clean run.
    drush_log("Pre-existing settings file found in site subdir. This can cause much confusion.", 'warning');
    if (!is_writable("${root}/${site_subdir}/settings.php")) {
      chmod("${root}/${site_subdir}/settings.php", 0775);
    }
    // If it exists and is valid, however, offer to re-use it.
    // This avoids busting up ADD or XAMMP or other local setups that may want
    // to be retained.
    //
    // If we have a local working environment already,
    // we can skip the from-scratch site-install, and re-use the db credentials.
    if (!empty($local_site_spec['#bootstrap'])) {
      // A valid D6 site is already in place here.
      // TODO Drupal7/8 response checks.
      drush_log("Pre-existing site seems valid. We will leave it in place and recycle it, using the same database.", 'warning');
    }
    else {
      drush_log("Pre-existing site seems broken. We will wipe and rebuild it..", 'warning');
      drush_log("We will replace it with a new local one.\nPer-instance site settings will be discarded, as we usually do not want live proxy, db, varnish, security etc settings down on local. If you do want them, you should update local.settings.php yourself.");
      drush_log("Need to discard existing $settings_file .", 'notice');
      $deleted = unlink($settings_file);
      if (file_exists($settings_file)) {
        drush_log("Failed to remove previous $settings_file . Sometimes permissions can cause trouble writing to this directory.", 'error');
      }
    }
  }

  // It's often necessary to set up some local settings overrides
  // on a local copy.
  // File path overrides.
  // Also optional stage_file_proxy settings (if enabled).

  $source_uri = $source_site_spec['uri'];
  $settings_local = "<" . "?" . "php
    // D7 compatibility
    \$conf['file_private_path']   = '${site_subdir}/private/files';
    \$conf['file_public_path']    = '${site_subdir}/files';
    \$conf['file_temporary_path'] = '${site_subdir}/private/temp';
    // D6 compatibility
    \$conf['file_directory_path'] =  \$conf['file_public_path'];
    \$conf['file_directory_temp'] =  \$conf['file_temporary_path'];
    
    \$conf['stage_file_proxy_origin'] = 'http://${source_uri}';
    \$conf['stage_file_proxy_hotlink'] = TRUE;
  ";

  if (!isset($source_site_spec['path-aliases']['%files'])) {
    drush_log('Need to double-check and retrieve the %files path from the remote site. Sorry for the delay.', 'notice');
    # Using drush_sitealias_evaluate_path() seems like mad overkill.
    # This is enough to prepopulate %files. A bit slow unfortunately.
    drush_sitealias_resolve_path_references($source_site_spec, '%files');
  }
  if (isset($source_site_spec['path-aliases']['%files'])) {
    $stage_file_proxy_origin_dir = $source_site_spec['path-aliases']['%files'];
    $settings_local .= "\n    \$conf['stage_file_proxy_origin_dir'] = '$stage_file_proxy_origin_dir';";
  }

  $saved = file_put_contents("${root}/${site_subdir}/settings.local.php", $settings_local);
  if (!$saved) {
    drush_log("Failed to update ${site_subdir}/settings.local.php . Sometimes permissions can cause trouble writing to this directory.", 'error');
  }

  // It's hard to edit the settings file before the first site-install has run.
  // It's easier to edit default.settings and let it get copied.
  $use_settings_local = "
    # Additional site configuration settings.
    if (file_exists(__DIR__ . '/settings.local.php')) {
      include_once(__DIR__ . '/settings.local.php');
    }
  ";
  $saved = file_put_contents("${root}/${site_subdir}/default.settings.php", $use_settings_local, FILE_APPEND);
  if (!$saved) {
    drush_log("Failed to update /${site_subdir}/default.settings.php . Sometimes permissions can cause trouble writing to this directory.", 'error');
  }

  // I thought site-install would have ensured these folders were set up.
  // But maybe not always.
  $dirs = [
    "${site_subdir}/files",
    "${site_subdir}/private",
    "${site_subdir}/private/files",
    "${site_subdir}/private/temp",
  ];
  foreach ($dirs as $dir) {
    if (!is_dir("${root}/$dir")) {
      mkdir("${root}/$dir");
    }
  }
  // If running locally as default sitedir, or under ADD, it's nice to leave a
  // hint in drushrc.php to help do things like drush uli.
  $local_uri = $local_site_spec['uri'];
  $drushrc = "<" . "?" . "php
    \$conf['uri'] = '${local_uri}';
  ";
  $saved = file_put_contents("${root}/${site_subdir}/drushrc.php", $drushrc);
}

function drush_get_configure_project_drush($source_site_spec, $local_site_spec) {
  // SITE ALIASES
  // As a developer shortcut, note the site identifier in local configs.
  // We can use this to avoid too much typing or remembering things.
  drush_log('Setting up some convenience shortcuts for drush aliases - putting them into the project/drush directory.', 'info');
  $project_dir = dirname($local_site_spec['root']);
  if (is_writable($project_dir)) {
    $drush_dir = $project_dir . DIRECTORY_SEPARATOR . 'drush';
    if (! is_dir($drush_dir)){
      mkdir($drush_dir);
    }
    // To help the project, generate site-aliases in the drush dir.
    // Use our own drush_get_save_alias routine to save that out.
    // Reset the global-ish save-alias path to do this.
    drush_set_option('alias-path', $drush_dir);
    // EG, to make /drush/mirror.alias.drushrc.php, set the #name.
    drush_get_save_alias($local_site_spec['instance'], $local_site_spec);
    $strings['%instance'] = $local_site_spec['instance'];
    drush_log(dt("You should be able to address this site instance as @%instance from the project context if you want.", $strings), 'info');

    // Don't copy & paste this one, use inheritance.
    $source_site_spec_wrapper = array('parent' => '@' . $source_site_spec['#name']);
    drush_get_save_alias('source', $source_site_spec_wrapper);
    drush_log(dt("You should be able to address this site instance as @source from the project context if you want.", $strings), 'info');

    // Even older shorthand for bash checks.
    $shorthand_filepath = $project_dir . DIRECTORY_SEPARATOR . $local_site_spec['instance'] . '.txt';
    file_put_contents($shorthand_filepath, '@' . $local_site_spec['uri']);
  }
}

/**
 * Prepare the site directory and settings.php inside it.
 *
 * Most of the downloads leading to here should NOT have produced a settings.php
 * as they are excluded by rsync, and (should be) ignored by source control.
 * So if one exists, it may be because
 * - we are re-downloading in place OR
 * - it's wrong.
 *
 * If it's wrong, we can find out by bootstrapping.
 * If it bootstraps, it smells like it's local and valid
 * (such as by XAMMP or ADD)
 * So check if we want to blow it away and remake, or recycle the exisiting.
 *
 * @param array $site_spec
 * @return bool
 */
function drush_get_site_setup($source_site_spec, $local_site_spec) {
  $alias = $strings['%alias'] = $local_site_spec['alias'];
  $subdir = preg_replace('#sites/#', '', $local_site_spec['path-aliases']['%site']);
  $strings['%subdir'] = $subdir;
  $strings['%local_db'] = $local_site_spec['db-url'];

  $site_install_args = array();
  $site_install_options = array(
    "--sites-subdir=" . $subdir,
  );
  $backend_options = array('interactive' => TRUE);

  // Adjust local file storage rules and prep local.settings.php.
  drush_get_configure_site_settings($source_site_spec, $local_site_spec);

  if (empty($local_site_spec['#bootstrap'])) {
    $site_install_options[] = "--db-url=" . $local_site_spec['db-url'];
  }
  else {
    // If there was a pre-existing working site, use the existing db creds
    // that are already in the settings.php.
    // This is so we don't fight with Aegir or ADD
    // over ownership of settings.php.
  }

  // When running drush_invoke_process, we may find ourselves running
  // a totally different version of drush!
  // Site instances may have a local drush - of a difference version even.
  // That forks into a cli process and everything.
  //
  // I actually do NOT want the full site-install,
  // But it's hella easier that just trying to invoke just the needed
  // database creation part of it.
  // I really just wanted drush_core_pre_site_install().
  drush_log(dt("About to initialize the site directory '%subdir' and a new database.", $strings), 'ok');
  // Needed the $alias drush context to keep site-install happy.
  $site_install_result = drush_invoke_process($alias, 'site-install', $site_install_args, $site_install_options, $backend_options);

  // This may return a response array.
  $site_install_success = !$site_install_result['error_status'];
  if ($site_install_success) {
    drush_log(dt("Site Install seems to have been a success. (Currently empty, DB to come)", $strings), 'success');
    drush_log(dt("You can probably run `drush %alias runserver --uri=@local_uri /` and see it working. ", $strings), 'ok');
  }
  else {
    drush_log(dt("Site Install seems to have failed.", $strings), 'error');
    print_r($site_install_result);
    $strings['@command'] = "drush $alias site-install " . implode(' ', $site_install_args) . ' ' . implode(' ', $site_install_options);
    drush_log(dt("You'll have to investigate why a command like\n  @command\n failed.", $strings), 'error');
    drush_log(dt("One reason can be file permissions on settings.php when downloading over an existing site.", $strings), 'notice');
    drush_log(dt("Depending where site-install failed, you should probably DROP the %local_db database before trying again.", $strings), 'notice');
    drush_log(dt("Also, some older install profiles just do not work with drush-based installs or variant versions of drush and php. If the database came up and you are going to replace it, this can probably be ignored.", $strings), 'notice');
    if (!drush_confirm("Do you want to continue anyway?")) {
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Perform the SQL Downsync
 *
 * @param $source_site_alias
 * @param $local_site_alias
 * @return bool
 */
function drush_get_database($source_site_alias, $local_site_alias) {
  $sql_sync_args = array(
    $source_site_alias,
    $local_site_alias,
  );
  $strings = array(
    '@source_site_alias' => $source_site_alias,
    '@local_site_alias' => $local_site_alias,
  );
  $sql_sync_options = array();
  $backend_options = array('interactive' => TRUE);
  drush_log(dt("About to sync the remote database down. This usually takes time. Effectively running `drush sql-sync @source_site_alias @local_site_alias'", $strings), 'ok');
  drush_log(dt("You can skip the sql-sync if you are planning to replace the databse later in another way.", $strings), 'ok');
  $sql_synced_result = drush_invoke_process('@self', 'sql-sync', $sql_sync_args, $sql_sync_options, $backend_options);
  $sql_synced_success = empty($sql_synced_result['error_status']);
  if ($sql_synced_success) {
    drush_log(dt("Completed sql-sync site DB from @source_site_alias to @local_site_alias. We now have a ready-to-run site.", $strings), 'success');
  }
  else {
    drush_log(dt("Failed to sql-sync site DB from @source_site_alias to @local_site_alias. This feels like a problem, so exiting", $strings), 'error');
    print_r($sql_synced_success);
    if (!drush_confirm("Do you want to continue anyway?")) {
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Perform the files rsync.
 *
 * @param $source_site_alias
 * @param $local_site_alias
 * @return bool
 */
function drush_get_user_files($source_site_alias, $local_site_alias) {
  // Optionally fetch the rest of the site user files.
  drush_print(dt("We previously avoided downloading the user files, as they can often be slow."));
  drush_print(dt("For a development mirror, you can often do without these, or use stage_file_proxy instead."));
  if (drush_confirm('Do you want to rsync the user files now?')) {
    $backend_options = array('interactive' => TRUE);
    $rsync_args = array(
      $source_site_alias . ':%files',
      $local_site_alias . ':%files',
    );
    $rsync_options = array(
      '--exclude-paths=backup_migrate,simpletest,styles',
    );
    drush_log(print_r([
      'command' => 'rsync',
      'args' => $rsync_args,
      'options' => $rsync_options,
      'backend' => $backend_options
    ], 1), 'debug');
    $rsynced_result = drush_invoke_process('@self', 'rsync', $rsync_args, $rsync_options, $backend_options);
    // I don't really care about success or failure at this point.
  }
  return TRUE;
}

/**
 * Guess a good shortname/slug.
 *
 * @param array $site_spec
 *
 * @return string
 */
function drush_get_short_name_from_site_spec($site_spec) {
  // The command may have explicitly defined a name to use for our project.
  // Use that if so.
  if ($projectname = drush_get_option('project-name', '')) {
    return $projectname;
  }

  // This is a non-standard addition that may be explicitly added to
  // custom site-aliases.
  // Note, 'name' is not '#name'.
  if (isset($site_spec['name'])) {
    return $site_spec['name'];
  }
  // Guess projectname from
  // Site-alias IDs may be like
  // dev.myproject.server.tld
  // myproject.tld.com
  // www.myproject.tld.com
  // dev_branch.myproject.server.tld
  // 20170707.myproject.server.tld.
  $name_parts = explode('.', $site_spec['#name']);
  if (in_array($name_parts[0], array(
    'www',
    'dev',
    'live',
    'test',
    'uat',
    'prod'
  ))) {
    array_shift($name_parts);
  }
  $short_name = array_shift($name_parts);
  return $short_name;
}

/**
 * Calculate a db connection URL
 *
 * @param array $site_spec
 *
 * @return string
 */
function db_pattern_from_site_spec($site_spec) {

  // You should set db_pattern in your own drushrc.php
  // Acquia dev desktop uses port 3306 or 3307 and uses drupaluser.
  $get_db_pattern = drush_get_option('get-db-pattern', 'mysql://%short-name_%instance:%random@127.0.0.1:3306/%short-name_%instance');
  $local_db = drush_get_process_tokens($get_db_pattern, $site_spec);

  // Username cannot be too long, or mysql fails. Need to repair that!
  $db_spec = drush_convert_db_from_db_url($local_db);
  $db_spec['username'] = substr($db_spec['username'], 0, 16);
  $db_url = drush_sitealias_convert_db_spec_to_db_url($db_spec);
  return $db_url;
}

// Some funcs are unavailable in drush <= 6. Load a backport stub lib.
if (intval(DRUSH_MAJOR_VERSION) < 7) {
  // provides:
  // drush_sitealias_convert_db_spec_to_db_url()
  require_once(__DIR__ . '/get.drush6.inc');
}
