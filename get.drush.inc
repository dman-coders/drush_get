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
 *
 * Ensures:
 * - your local paths are appropriate
 * - your connection to your local db is configured
 * - the expected master-alias site is available.
 */
function drush_get_setup() {
  drush_log(dt("About to review your local setup. This will probe your local storage path for projects, your access to your own database, connections to your remote list of projects, and the status there.", array()), 'info');

  $out = array();
  $master_alias = drush_get_option('master-alias', '');
  $strings['@master-alias'] = $master_alias;

  // Check local environment preferences.
  // docroot is needed.
  $get_docroot_pattern = drush_get_option('get-docroot-pattern', '/var/www/%short-name/%docroot-name');
  $sampleproject = array(
    'uri' => 'sampleproject.remotehost.demo',
    'name' => 'sampleproject',
    'role' => 'dev',
  );
  $local_root = drush_get_process_tokens($get_docroot_pattern, $sampleproject);
  $strings['@get-docroot-pattern'] = $get_docroot_pattern;
  $strings['@local_root'] = $local_root;
  drush_log(dt("Your generic docroot pattern is @get-docroot-pattern . This can be adjusted in your own drushrc.php, see 'drush topic get' for more.", $strings), 'success');
  drush_log(dt("Locally copied sites will be saved in paths such as @local_root .", $strings), 'success');


  // Admin database-superuser login is needed.
  $db_su = drush_get_option('db-su', 'root');
  // You should set db_pattern in your own drushrc.php
  // Acquia dev desktop uses port 3306 or 3307 and uses drupaluser.
  $get_db_pattern = drush_get_option('get-db-pattern', 'mysql://%short-name:%random@localhost:3306/%short-name');
  $local_db = drush_get_process_tokens($get_db_pattern, $sampleproject);
  $strings['@db-su'] = $db_su;
  $strings['@get-db-pattern'] = $get_db_pattern;
  $strings['@local_db'] = $local_db;
  drush_log(dt("Your generic database connection pattern is @get-db-pattern and your database admin user is @db-su . This can be adjusted in your own drushrc.php, see 'drush topic get' for more.", $strings), 'success');
  drush_log(dt("Locally copied sites will be stored in the database identified like @local_db .", $strings), 'success');

  // Expected configs used and process should be as much like
  // drush_core_site_install()
  // as possible.
  // $db_spec is a d7 style db info array.
  $db_spec = drush_convert_db_from_db_url($local_db);

  drush_log(dt("Testing to see if we have permissions to make a database.", $strings), 'success');
  // This is the API call we should use later, so test it directly now.
  // $success = drush_sql_empty_db($db_spec);
  drush_log(dt('Testing our DB spec has appropriate access.'), 'ok');
  /**
   * @var $sql \Drush\Sql\SqlBase
   */
  $sql = drush_sql_get_class($db_spec);
  try {
    $sql->drop_or_create();
  }
  catch (Error $e) {
    return drush_set_error(dt('Failed to create database: @error', array('@error' => $e->getMessage())));
  }

  drush_log(dt('Looks like we had the ability to make databases.'), 'ok');

  // The test is if we can use the new credentials to create a table;
  // Try that now.
  $success = FALSE;
  try {
    $success = $sql->query(sprintf('DROP TABLE IF EXISTS %s;', 'deleteme'));
    $success &= $sql->query(sprintf('CREATE TABLE %s (id INT);', 'deleteme'));
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
    return FALSE;
  }
  // Clean up.
  drush_log(dt("Removing test database.", $strings), 'debug');
  $sql->su();
  $sql->query(sprintf('DROP DATABASE IF EXISTS %s;', $db_spec['database']));

  drush_log(dt("Local Database info and access seems fine.", $strings), 'ok');


  drush_log(dt("Now checking master-alias for upstream info about sites.", $strings));
  if (empty($master_alias)) {
    drush_log("No master-alias is set", 'warning');
    drush_print("You should define your default master-alias option in your drushrc.php");
    drush_print("The master-alias identifies a site on your central server that contains a list of all known site-aliases.");
    drush_print("@hostmaster.aegir.devserver would be good. It must be a full bootstrapped site, not just a server login.");
    drush_print("See 'drush docs-get' topic for more.");
  }
  else {
    drush_log(dt("master-alias is @master-alias", $strings), 'info');
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
      drush_log(dt("master-alias is to be found at @remote-user@@remote-host", $strings), 'info');

      // Test connection to master-alias.
      drush_print(dt('Checking connection to "@remote-host" and the drush version there.', $strings));

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
      drush_log(dt('Remote connection succeded and drush version on "@remote-host" was reported as @remote_drush_version.', $strings), 'success');

      // Boring. The way data was returned is different between drush 5 & 6.
      $drush_bootstrap_key = 'bootstrap';
      if ($remote_drush_version < 6) {
        drush_log(dt('Some features of the drush_get tool have only been found to work against drush 6+ . Lower versions MAY work, but the diagnostics we have to work with are more likely to fail. I suggest installing a higher version of drush, and checking the drush-script for the target server.', $strings), 'warning');
        $drush_bootstrap_key = 'Drupal bootstrap';
      }


      drush_print(dt('Checking availability and status of master-alias site @master-alias', $strings));
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
        drush_log(dt('Connection to master-alias @master-alias and Drupal bootstrap succeeded.', $strings), 'success');
        print_r($lookup_results['object']);
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
 * @param array $site_alias
 *   A site alias containing a number of keys that could be useful as
 *   substititions.
 *
 * @return string
 *   The processed result.
 */
function drush_get_process_tokens($pattern, $site_alias) {
  $strings = array();
  // Copy all strings and paths as substitute tokens.
  foreach ($site_alias as $key => $value) {
    if (is_string($value)) {
      $strings['%' . $key] = $value;
    }
  }
  $strings += (array) $site_alias['path-aliases'];

  if (!isset($strings['%short-name'])) {
    if (isset($strings['%name'])) {
      $strings['%short-name'] = $strings['%name'];
    }
    else {
      $strings['%short-name'] = $strings['%uri'];
    }
  }
  // Shortname must always be safe for filesystem and database identifiers.
  $strings['%short-name'] = preg_replace('/[^a-z0-9]+/', '-', strtolower($strings['%short-name']));


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
 * Optionally store the info locally.
 *
 * @param string $site_alias
 *   Site identifier.
 *
 * @return array
 *   site-alias
 */
function drush_get_whois($site_alias) {
  // nb.
  // _alias is the string identifier,
  // _spec is the array that it represents.
  // _string is the text version of that that the remote call may give us.
  $strings = array(
    '@site-alias' => $site_alias,
  );
  $master_alias = drush_get_option('master-alias', '');
  $strings['@master-alias'] = $master_alias;
  $master_spec = drush_sitealias_get_record($master_alias);

  if (drush_get_option('refresh', FALSE)) {
    // Check the local list of known aliases first.
    $site_spec = drush_sitealias_get_record($site_alias);
    $site_spec_string = '$aliases["' . $site_spec['#name'] . '"] = ' . var_export($site_spec, TRUE) . ';' . PHP_EOL;
    if (!empty($site_spec)) {
      drush_log(dt('Local record of @site-alias found.', $strings), 'OK');
    }
  }

  if (empty($site_spec)) {
    if (!empty($master_spec)) {
      // Query the master-alias for this info.
      drush_log(dt('Asking @master-alias if it knows about @site-alias', $strings), 'info');
      $lookup_results = drush_invoke_process($master_alias, 'site-alias', array($site_alias), array('format' => 'config'), FALSE);
      // This returns the site_alias snippet in the $lookup_results['output'];
      // Notably, it does not return an array of data?
      // Output is empty string on failure.
      if (!empty($lookup_results['output'])) {
        drush_log(dt('Retrieved site spec of @site-alias from @master-alias ok.', $strings), 'info');
        $site_spec_string = $lookup_results['output'];
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
    $overwrite = drush_get_option('refresh');
    drush_get_save_alias($site_alias, $site_spec_string, $overwrite);
  }
print_r($site_spec_string);
  return $site_spec;
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
 * @return bool
 *   Success.
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
    #unlink($alias_filepath);
    return FALSE;
  }

  drush_log(dt('Saved the alias info to @alias-filepath', $strings), 'OK');
  return TRUE;
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
  /*
  if (!$alias = drush_get_context('DRUSH_TARGET_SITE_ALIAS')) {
    return drush_set_error('DRUSH_MISSING_TARGET_ALIAS', 'A remote site alias is required.');
  }
  */

  foreach ($site_list as $site_alias) {
    $strings['@site-alias'] = $site_alias;
    drush_log(dt("Attempting to fetch @site_alias", $strings), 'info');
    // $site_id will be the @site.alias .
    $source_site_spec = drush_sitealias_get_record($site_alias);
    if (empty($source_site_spec)) {
      drush_log(dt('Could not find a site-alias entry for @site-alias . Check the ID, or try "drush get-whois -v @site-alias" for more details.', $strings), 'error');
      continue;
    }
    $strings['@root'] = $source_site_spec['root'];
    $strings['@remote-host'] = $source_site_spec['remote-host'];
    drush_log(dt('Info retrieved describing @site-alias. It is in @root on @remote-host .', $strings), 'error');
    print_r($source_site_spec);
  }

}
