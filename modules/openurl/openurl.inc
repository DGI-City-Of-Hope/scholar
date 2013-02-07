<?php

/**
 * @file
 *
 * Provides some convience functions for generating OpenURL links.
 */
module_load_include('inc', 'openurl', 'openurl.db');

/**
 * Given a named OpenURL link, created on the settings page for this module.
 * Generate a OpenURL combinding the given parameters with the stored default parameters.
 *
 * @param string $name
 *   The name of the OpenURL as defined in the settings page for this module.
 * @param array $params
 *   The set of key values parameters to use with OpenURL.
 *
 * @return string
 *   A OpenURL link if successful FALSE otherwise.
 */
function openurl_link($name, array $params = array()) {
  if (($result = openurl_db_get_setting($name)) != FALSE) {
    $setting = db_fetch_object($result);
    $default_params = openurl_get_default_params($name);
    $params = array_merge($default_params, $params);
    return openurl_url($setting->url, $params);
  }
  return FALSE;
}

/**
 * Gets the default parameters to be used with the given OpenURL
 *
 * @param string $name
 *   The name of the OpenURL as defined in the settings page for this module.
 *
 * @return array
 *   The default key value pairs to be used with the given OpenURL.
 */
function openurl_get_default_params($name) {
  $params = array();
  $result = openurl_db_get_default_params($name);
  while ($param = db_fetch_object($result)) {
    $params[] = array($param->param_key => $param->param_value);
  }
  return $params;
}

/**
 * Creates an OpenURL from the given base resolver URL and key value parameters.
 *
 * @param string $url
 *   The base url for the link resolver.
 * @param array $params
 *   The params to use in constructing the openurl url.
 */
function openurl_url($url, array $params) {
  return $url . '?' . http_build_query($params);
}

/**
 * Theme's a OpenURL link.
 *
 * @param string $name
 *   A OpenURL link,
 * @param string $icon
 *   A path to an option icon to display as the OpenURL link.
 *
 * @return string
 *   The html link.
 */
function theme_openurl_link($name, array $params = array(), $title = 'link', $text='link') {
  $url = openurl_link($name, $params);
  $settings = db_fetch_object(openurl_db_get_setting($name));
  $icon = FILES_OPENURL . '/' . $settings->icon;
  if (isset($settings->icon) && file_check_location($icon)) {
    $icon = url($icon);
    return "<a href='{$url}' title='{$title}' onClick=\"window.open('{$url}', 'OpenURL','height=600,width=800,modal=yes,alwaysRaised=yes'); event.returnValue = false; return false;\"><img src='{$icon}' alt='{$title}'></a>";
  }
  else {
    return "<a href='{$url}' title='{$title}' onClick=\"window.open('{$url}', 'OpenURL','height=600,width=800,modal=yes,alwaysRaised=yes'); event.returnValue = false; return false;\">$text</a>";
  }
}
