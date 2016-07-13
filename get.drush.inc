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
 * Requires Drush 7+
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
      'field-labels' => array('#name' => 'Name', 'root' => 'Root', 'uri' => 'URI', 'remote-host' => 'Host', 'remote-user' => 'User', 'remote-port' => 'Port', 'os' => 'OS', 'ssh-options' => 'SSH options', 'php' => 'PHP'),
      'fields-default' => array('#name', 'root', 'uri', 'remote-host', 'remote-user'),
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
  drush_log(dt("#### drush-get patterns ####"));

  $out = array();
  $master_alias = drush_get_option('master-alias', '');
  $strings['@master-alias'] = $master_alias;

  // Check local environment preferences.
  $sampleproject_spec = array(
    'uri' => 'sampleproject.remotehost.demo',
    'name' => 'sampleproject',
    'role' => 'dev',
  );
  $get_docroot_pattern = drush_get_option('get-docroot-pattern', '/var/www/%short-name/%docroot-name');
  $local_root = drush_get_process_tokens($get_docroot_pattern, $sampleproject_spec);
  $strings['@get-docroot-pattern'] = $get_docroot_pattern;
  $strings['@local_root'] = $local_root;

  $get_uri_pattern = drush_get_option('get-uri-pattern', 'mirror.%short-name.localhost');
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

  // You should set db_pattern in your own drushrc.php
  // Acquia dev desktop uses port 3306 or 3307 and uses drupaluser.
  $get_db_pattern = drush_get_option('get-db-pattern', 'mysql://%short-name:%random@localhost:3306/%short-name');
  $local_db = drush_get_process_tokens($get_db_pattern, $sampleproject_spec);
  $strings['@get-db-pattern'] = $get_db_pattern;
  $strings['@local_db'] = $local_db;
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
  }
  catch (Error $e) {
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
    // $sql->query(sprintf('DROP TABLE IF EXISTS %s;', 'deleteme'));
    $success = $sql->query(sprintf('CREATE TABLE %s (id INT);', 'deleteme'));
  }
  catch (Error $e) {
    return drush_set_error(dt('Failed to create table in database: @error', array('@error' => $e->getMessage())));
  }

  if ($success) {
    drush_log(dt("Looked like we could add a database and grant permissions on it and work on it.", $strings), 'success');
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
        //print_r($lookup_results['object']);
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

  return $out;
}

/**
 * Calculate the appropriate storage path for a local project.
 *
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
  $strings += @(array)$site_spec['path-aliases'];

  if (!isset($strings['%short-name'])) {
    if (isset($strings['%name'])) {
      $strings['%short-name'] = $strings['%name'];
    }
    else {
      $strings['%short-name'] = $strings['%uri'];
    }
  }
  // short-name must always be safe for filesystem and database identifiers.
  $strings['%short-name'] = preg_replace('/[^a-z0-9]+/', '-', strtolower($strings['%short-name']));

  // safe-uri can be used for longer slugs
  // - Good for a database name if shortname is reserved.
  $strings['%safe-uri'] = preg_replace('/[^a-z0-9]+/', '_', strtolower($strings['%uri']));

  if (!isset($strings['%docroot-name'])) {
    if (isset($strings['%role'])) {
      $strings['%docroot-name'] = $strings['%role'];
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

  if (drush_get_option('refresh', FALSE)) {
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

  // Optionally save that config locally for next time.
  if (drush_get_option('save', '')) {
    // Build the PHP string.
    $aliases[$site_name] = $site_spec;
    $site_spec_string = var_export($aliases, TRUE) . ';' . PHP_EOL;

    $overwrite = drush_get_option('refresh');
    $saved = drush_get_save_alias($site_alias, $site_spec_string, $overwrite);
    if (!$saved) {
      drush_log(dt('Did not save the site-alias to the expected site-alias directory.', $strings), 'error');
    }

    /*
    // Maybe we could eval that string now,
    // But instead - use our own API to read back in the site spec
    // that we just wrote.
    $strings['%alias_directory'] = dirname($saved);
    // It should resolve now.
    $site_spec = drush_sitealias_get_record($site_alias);
    if (empty($site_spec)) {
      drush_log(dt('Failed to read the site specification (@site-alias) that I thought I just saved. Could be that we are configured to save site-aliases into a directory that we are not configured to read from. That is probably no help. Ensure that the directory %alias_directory is configured to be in your drush site-alias search paths.', $strings), 'error');
    }
    */
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
 * @param string $site_spec_string
 *   PHP config file snippet.
 * @param bool $overwrite
 *   Whether to overwrite an existing file found there.
 *
 * @return bool|string
 *   The successfully saved filepath. FALSE on failure.
 */
function drush_get_save_alias($site_alias, $site_spec_string, $overwrite = FALSE) {
  if (empty($site_alias) || empty($site_alias)) {
    drush_log(dt('Empty parameters given to @FUNCTION', array('@FUNCTION' => __FUNCTION__)), 'warning');
    return FALSE;
  }
  // OK, if you gave me a site_spec array, I can deal with that also.
  // Cast it to a config string on the fly here.
  if (is_array($site_spec_string) && !empty($site_spec_string['#name'])) {
    $site_spec_string = '$aliases["' . $site_spec_string['#name'] . '"] = ' . var_export($site_spec_string, TRUE) . ';' . PHP_EOL;
  }

  $alias_path = drush_get_option('alias-path', '');
  if (is_array($alias_path)) {
    $alias_path = reset($alias_path);
  }
  $clean_site_alias = ltrim($site_alias, '@');
  $alias_filepath = $alias_path . DIRECTORY_SEPARATOR . $clean_site_alias . '.alias.drushrc.php';
  $strings['@alias-filepath'] = $alias_filepath;

  if (file_exists($alias_filepath) && !$overwrite) {
    drush_log(dt('An alias file for this site already exists at @alias-filepath . Use the --refresh option if you really want to overwrite it.', $strings), 'warning');
    return FALSE;
  }

  // Prepare and save that info locally.
  $file_contents = '<?php' . PHP_EOL . $site_spec_string;
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
    drush_log(dt('Syntax check on the site-alias config code failed. This is unexpected and indicates a problem with the data supplied from upstream. Run this command with --debug on for clues.', $strings), 'error');
    drush_log(dt('Removing the broken @alias-filepath code file for now.', $strings), 'warning');
    unlink($alias_filepath);
    return FALSE;
  }

  drush_log(dt('Saved the alias info to @alias-filepath', $strings), 'ok');
  return $alias_filepath;
}

/**
 * Pulls down a copy of an existing site and sets up a clone locally.
 *
 * A drush command.
 *
 * @return NULL
 */
function drush_get_site() {
  $strings = array();
  $site_list = func_get_args();
  if (empty($site_list)) {
    drush_log(dt('No site given', $strings), 'warning');
    return NULL;
  }

  foreach ($site_list as $source_site_alias) {
    $strings['@source_site_alias'] = $source_site_alias;
    drush_log(dt("Attempting to fetch @source_site_alias", $strings), 'info');
    // $site_id will be the @site.alias .
    $source_site_spec = drush_sitealias_get_record($source_site_alias);
    if (empty($source_site_spec)) {
      drush_log(dt('Could not find a site-alias entry for @source_site_alias . Check the ID, or try "drush get-whois -v @source_site_alias" for more details.', $strings), 'error');
      continue;
    }
    $strings['@root'] = $source_site_spec['root'];
    $strings['@remote-host'] = $source_site_spec['remote-host'];
    drush_log(dt('Info retrieved describing @source_site_alias. It is in @root on @remote-host .', $strings), 'success');

    // So far so good.
    // Now to do :
    // * Local platform setup, using the get_setup preferences.
    // * A file-copy down
    // * Database sync down
    // * Local status check.

    // Build the local site site spec, derived from the remote one.
    // Guess a good shortname/slug.
    $name_parts = explode('.', $source_site_spec['#name']);
    if ($name_parts[0] == 'www' || $name_parts[0] == 'dev' || $name_parts[0] == 'live') {
      array_shift($name_parts);
    }
    $short_name = array_shift($name_parts);
    $strings['@short_name'] = $short_name;

    // Derive the local dev site definition.
    $local_site_spec = array(
      'name' => $short_name,
      'role' => 'dev',
      'path-aliases' => array(),
    );

    $get_uri_pattern = drush_get_option('get-uri-pattern', 'mirror.%short-name.localhost');
    $local_uri = drush_get_process_tokens($get_uri_pattern, $local_site_spec);
    $local_site_spec['uri'] = $local_uri;
    $strings['@local_uri'] = $local_uri;

    $get_docroot_pattern = drush_get_option('get-docroot-pattern', '/var/www/%short-name/%docroot-name');
    $local_root = drush_get_process_tokens($get_docroot_pattern, $local_site_spec);
    $local_site_spec['root'] = $local_root;
    $strings['@local_root'] = $local_root;

    // A path-based local_root_alias is enough to start doing other processes.
    $local_site_alias = "$local_root#$local_uri";
    $strings['@local_site_alias'] = $local_site_alias;
    drush_log(dt("Your generic project will be short_name @short_name.", $strings), 'ok');
    drush_log(dt("Locally copied site @local_uri will be deployed into @local_root .", $strings), 'ok');

    // Initiate rsync of the remote real site (ignore VCS , get the real thing).
    $rsync_args = array(
      $source_site_alias,
      $local_site_alias,
    );
    $rsync_options = array(
      // Recent version of drush stopped assuming yes when dispatching
      // and defaulted to no??

      # If I define exclude-paths here,
      # it seems to override the site-alias ones.
      #  '--exclude-paths=%files'

      // In order to speed up bootstrapping, exclude sites folders during
      // this phase of mirroring.
      // Hm, but this leaves behind 'default'
      // and hence default/default.settings.php
      // hence cannot run site-install.
      // '--exclude-sites'
    );

    drush_log(dt("About to rsync ALL FILES from @source_site_alias to @local_site_alias. This is likely to take a while with big sites. This is a raw file copy, mirroring the real site, not using source control, and only excluding content that has already bee excluded via the source sites site-alias settings.", $strings), 'ok');
    if (!drush_mkdir($local_root, TRUE)) {
      drush_log(dt("Could not prepare local root directory @local_root to copy into. Probably permissions. Aborting", $strings), 'error');
      continue;
    }
    $rsynced_success = drush_invoke_process('@self', 'rsync', $rsync_args, $rsync_options);
    if ($rsynced_success && !$rsynced_success['error_status']) {
      // TODO - more checking - seems possible to get a success flagged,
      // but no actual files. Do you really want to continue? (y/n): Cancelled.
      drush_log(dt("Completed rsync site files from @source_site_alias to @local_site_alias. We now have a platform to build on.", $strings), 'success');
    }
    else {
      if ($rsynced_success['error_status']) {
        foreach($rsynced_success['error_log'] as $log) {
          drush_log($log, 'error');
        }
      }
      drush_log(dt("Failed to rsync site files from @source_site_alias to @local_site_alias. This feels like a problem, so exiting", $strings), 'error');
      continue;
    }

    // Now initialize a site directory in the new platform.
    // What will the new db connection be?
    $get_db_pattern = drush_get_option('get-db-pattern', 'mysql://%short-name:%random@localhost:3306/%short-name');
    $local_db = drush_get_process_tokens($get_db_pattern, $local_site_spec);

    $site_install_args = array(
    );
    $site_install_options = array(
      "--db-url=$local_db",
      "--sites-subdir=$local_uri",
    );
    
    // When running drush_invoke_process, we may find ourselves running
    // a totally different version of drush!
    // Site instances may have a local drush - of a difference version even.
    // That forks into a cli process and everything.

    // I actually do NOT want the full site-install,
    // But it's hella easier that just trying to invoke just the needed
    // database creation part of it.
    // I really just wanted drush_core_pre_site_install().
    drush_log(dt("About to initialize the site directory and a new database.", $strings), 'ok');
    $site_install_success = drush_invoke_process($local_site_alias, 'site-install', $site_install_args, $site_install_options);

    if ($site_install_success) {
      drush_log(dt("Site Install seems to have been a success.", $strings), 'success');
      drush_log(dt("You can probably run `drush @local_site_alias runserver` and see it working.", $strings), 'ok');
    }
    else {
      drush_log(dt("Site Install seems to have failed.", $strings), 'error');
      $strings['@command'] = "drush $local_site_alias site-install " . implode(' ', $site_install_args) . ' ' . implode(' ', $site_install_options);
      drush_log(dt("You'll have to investigate why a command like\n  command\n failed.", $strings), 'error');
      drush_log(dt("Depending where site-insatll failed, you should probably DROP the %local_db database before trying again.", $strings), 'error');
      continue;
    }

    // Almost there - just the database sync left.
    $sql_sync_args = array(
      $source_site_alias,
      $local_site_alias,
    );
    $sql_sync_options = array(
    );
    drush_log(dt("About to sync the remote database down. This usually takes time.", $strings), 'ok');
    $sql_synced_success = drush_invoke_process('@self', 'sql-sync', $sql_sync_args, $sql_sync_options);
    if ($sql_synced_success) {
      drush_log(dt("Completed sql-sync site DB from @source_site_alias to @local_site_alias. We now have a ready-to-run site.", $strings), 'success');
    }
    else {
      drush_log(dt("Failed to sql-sync site DB from @source_site_alias to @local_site_alias. This feels like a problem, so exiting", $strings), 'error');
      continue;
    }


  }

}
