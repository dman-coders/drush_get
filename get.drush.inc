<?php
/**
 * @file
 * Utility for quick mirroring of sites to local dev environments.
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
    'description' => dt('Take a local copy of the named site.'),
    'arguments' => array(
      'source-site-alias' => 'Original site alias to be copied. If the site-alias is not yet known, the --master-alias instance will be queried to try and find it.',
      'example-value' => 'dev.projectname.sharedserver',
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
    'callback arguments' => array(dirname(__FILE__). '/README.md'),
  );
  return $items;
}

/**
 * Execute `drush get-setup` command.
 *
 * Self-test and report.
 *
 * Ensures the expected master-alias site is available.
 */
function drush_get_setup() {
  $out = array();
  $master_alias = drush_get_option('master-alias', '');
  $strings['@master-alias'] = $master_alias;

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
      drush_print(dt('Checking availability of @master-alias', $strings));
      $lookup_results = drush_invoke_process($master_alias, 'status', array(), array(), FALSE);
      // $lookup_results['output'] = "raw text"
      // $lookup_results['object'] = array() # Array version of that.
      // $lookup_results['error_status'] = 0 # If lucky.
      // A success would be:
      if ($lookup_results['object']['Drupal bootstrap'] == 'Successful') {
        drush_log(dt('Connection to master-alias @master-alias succeeded', $strings), 'ok');
      }
      else {
        drush_log(dt('Connection to master-alias @master-alias failed', $strings), 'error');
      }

    }
  }

  return $out;
}

/**
 * Execute `drush get-whois` command.
 *
 * If the named site alias is unknown, query the master-alias site for it.
 *
 * Optionally store the info locally.
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
    $site_spec_string = '$aliases["' . $site_spec['#name'] . '"] = ' . var_export($site_spec , TRUE) . ';' . PHP_EOL;
    if (!empty($site_spec)) {
      drush_log(dt('Local record of @site-alias found.', $strings), 'OK');
      drush_print_r($site_spec_string);
    }
  }
  #drush_log('Result was ' . $site_spec, 'info');

  if (empty($site_spec)) {
    if (!empty($master_spec)) {
      // Query the master-alias for this info.
      drush_log(dt('Asking @master-alias if it knows about @site-alias', $strings), 'info');
      $lookup_results = drush_invoke_process($master_alias, 'site-alias', array($site_alias), array('format' => 'config'), FALSE);
      // This returns the site_alias snippet in the $lookup_results['output'];
      // Notably, it does not return an array of data?
      // Output is empty string on failure.
      drush_print_r($lookup_results);
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

  return $site_spec;
}

/**
 * Save a given site spec into our default site-aliases folder.
 *
 * @param string $site_alias
 *   Site identifier.
 * @param string $site_spec
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
    $site_spec_string = '$aliases["' . $site_spec_string['#name'] . '"] = ' . var_export($site_spec_string , TRUE) . ';' . PHP_EOL;
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
  $file_contents = '<' . '?' . 'php' . PHP_EOL . $site_spec_string;
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

  foreach ($site_list as $site_id) {
    $strings['%site_id'] = $site_id;
    drush_log("Attempting to fetch %site_id", $strings);
    // $site_id will be the @site.alias .
    $source_site = drush_sitealias_get_record($site_id);
  }

}
