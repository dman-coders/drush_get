<?php

/**
 * Backport wrapper for trivial drush commands that were unavailable in drush6.
 *
 * @see drush topic docs-commands
 *
 * You may need to run drush6 cc drush to register this backport
 * if swapping between drushes.
 *
 * The idea is that this file will be found if DRUSH_MAJOR_VERSION==6
 * @see _drush_add_commandfiles()
 *
 * It seems that the promised version-specific autoload is not triggering.
 * The main get.drush.inc has to fetch me itself.
 *
 * We want to develop for modern drush BUT for some site-sync operations
 * it's easier to just use drush6 on the client in order to talk to old drush6
 * on the target.
 * So this util will avoid modernisms or backport them if really needed.
 *
 * My db_pattern_from_site_spec()
 * requires drush8 drush_sitealias_convert_db_spec_to_db_url()
 */

/**
 * Backport copy of func from drush8 commands/core/sitealias.drush.inc
 *
 */
function drush_sitealias_convert_db_spec_to_db_url($db_spec) {
  $result = urlencode($db_spec["driver"]) . "://";
  if (isset($db_spec["username"])) {
    $result .= urlencode($db_spec["username"]);
    if (isset($db_spec["password"])) {
      $result .= ":" . urlencode($db_spec["password"]);
    }
    $result .= "@";
  }
  // Host is required, unless this is an sqlite db.
  if (isset($db_spec["host"])) {
    $result .= urlencode($db_spec["host"]);
    if (isset($db_spec["port"])) {
      $result .= ":" . urlencode($db_spec["port"]);
    }
    $result .= '/' . urlencode($db_spec["database"]);
  }
  else {
    // URL-encode the database, but convert slashes
    // back to their original form for readability.
    // This portion is the "path" of the URL, so it may
    // contain slashes.  This is important for sqlite.
    $result .= str_replace("%2F", "/", urlencode(ltrim($db_spec["database"], '/')));
  }
  return $result;
}

