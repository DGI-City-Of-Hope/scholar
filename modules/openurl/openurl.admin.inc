<?php

/**
 * @file
 *
 * Implements hooks and callbacks for this module settings page.
 */
module_load_include('inc', 'openurl', 'openurl.db');

/**
 * Gets the form defintion for the settings page.
 *
 * @param array $form_state
 *   Drupal form state.
 *
 * @return array
 *   Drupal form definition.
 */
function openurl_settings_form(array &$form_state) {
  $settings = array(
    '#header' => array('Name', 'Base URL', 'Icon', 'Default', '', ''),
    '#theme' => THEME_OPENURL_FORM_TABLE,
  );
  $results = openurl_db_get_settings();
  while ($row = db_fetch_object($results)) {
    unset($row->id); // Don't display the id.
    $column_values = array();
    foreach ($row as $key => $value) {
      $column_values[$key] = array(
        '#value' => $value
      );
    }
    /**
     * @todo Determine how to get the default one.
     */
    $default = variable_get('openurl_default', NULL) == $row->name;
    $is_default = array(
      '#prefix' => '<b>',
      '#value' => t('Is Default'),
      '#suffix' => '</b>'
    );
    $set_default = array(
      '#type' => 'submit',
      '#value' => t('Set Default'),
      '#setting_name' => $row->name,
      '#name' => 'set-default-' . $row->name,
      '#validate' => array(), // Prevent default valiation.
      '#submit' => array('openurl_set_default'),
    );
    $settings[] = array_merge($column_values, array(
      '#tree' => TRUE,
      'default' => $default ? $is_default : $set_default,
      'edit' => array(
        '#type' => 'submit',
        '#value' => 'Edit',
        '#setting_name' => $row->name,
        '#name' => 'edit-' . $row->name,
        '#validate' => array(), // Prevent default valiation.
        '#submit' => array('openurl_edit_setting_redirect'),
      ),
      'delete' => array(
        '#type' => 'submit',
        '#value' => 'Delete',
        '#setting_name' => $row->name,
        '#name' => 'delete-' . $row->name,
        '#validate' => array(), // Prevent default valiation.
        '#submit' => array('openurl_delete_setting'),
        ))
    );
  }
  $add = array(
    '#type' => 'submit',
    '#value' => 'Add',
    '#submit' => array('openurl_add_setting'),
    '#validate' => array('openurl_add_setting_validate'),
  );
  $settings['add'] = array(
    'add_name' => array(
      '#type' => 'textfield',
      '#title' => 'Name',
      '#description' => t('Must be unique'),
      '#size' => 32,
      '#default_value' => isset($form_state['values']['add_name']) ? $form_state['values']['add_name'] : NULL,
    ),
    'add_url' => array(
      '#type' => 'textfield',
      '#title' => 'Base URL',
      '#description' => t('Link Resolver Address'),
      '#size' => 32,
      '#default_value' => isset($form_state['values']['add_url']) ? $form_state['values']['add_url'] : NULL,
    ),
    'file' => array(
      '#type' => 'file',
      '#title' => 'Icon',
      '#description' => t('Icon for the link.'),
      '#default_value' => isset($form_state['values']['file']) ? $form_state['values']['file'] : NULL,
    ),
    'submit' => $add,
    0 => array(
      '#value' => '' // Filler so the table is formated correctly.
    ),
    1 => array(
      '#value' => '' // Filler so the table is formated correctly.
    ),
  );
  $form = array(
    '#attributes' => array('enctype' => 'multipart/form-data'),
    'fieldset' => array(
      '#title' => t('Settings'),
      '#type' => 'fieldset',
      /**
       * A bit of a hack, I want the default action here to be add not delete, so I've mirrored the add button and
       * placed it at the begining of the form. Making it hidden, so it will be the default action.
       */
      'add' => array_merge($add, array('#attributes' => array('style' => 'position: absolute; left: -9999px; width: 1px; height: 1px;'))),
      'settings' => $settings
    ),
  );
  return $form;
}

/**
 * Validates the input for adding a new setting.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function openurl_add_setting_validate(array $form, array &$form_state) {
  $name = $form_state['values']['add_name'];
  if (empty($name)) {
    form_set_error('add_name', t('field is required.'));
  }
  if (openurl_db_has_setting($name)) {
    form_set_error('add_name', t('Settings for "%name" already exists.', array('%name' => $name)));
  }
  $validators = array('file_validate_is_image' => array());
  $file = file_save_upload('file', $validators, FILES_OPENURL, FILE_EXISTS_REPLACE);
  file_set_status($file, FILE_STATUS_PERMANENT);
  $form_state['values']['file'] = $file;
}

/**
 * Adds a new setting for OpenURL.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function openurl_add_setting(array $form, array &$form_state) {
  $values = $form_state['values'];
  $filename = isset($values['file']->filename) ? $values['file']->filename : NULL;
  if (openurl_db_add_setting($values['add_name'], $values['add_url'], $filename)) {
    $message = 'Successfully added settings for "%name"';
    $type = 'status';
  }
  else {
    $message = 'Failed to add settings for "%name"';
    $type = 'error';
  }
  $message_args = array('%name' => $values['add_name']);
  drupal_set_message(t($message, $message_args), $type);
}

/**
 * Redirects the user to the edit page for a given element.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function openurl_edit_setting_redirect(array $form, array &$form_state) {
  $name = $form_state['clicked_button']['#setting_name'];
  drupal_goto(str_replace('%', $name, MENU_OPENURL_DEFAULT_PARAMETERS));
}

/**
 * Deletes the given settings and redirects the user to the main settings page for OpenURL.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function openurl_delete_setting(array $form, array &$form_state) {
  $name = $form_state['clicked_button']['#setting_name'];
  if (openurl_db_delete_setting($name)) {
    $message = 'Successfully removed the settings for "%name"';
    $type = 'status';
  }
  else {
    $message = 'Failed to remove the settings for "%name"';
    $type = 'error';
  }
  $message_args = array('%name' => $name);
  drupal_set_message(t($message, $message_args), $type);
  drupal_goto(MENU_OPENURL_SETTINGS);
}

/**
 * Sets the default OpenURL to use when none is specified.
 *
 * @param array $form
 *   The Drupal form.
 * @param array $form_state
 *   The Drupal form state.
 */
function openurl_set_default(array $form, array &$form_state) {
  $name = $form_state['clicked_button']['#setting_name'];
  $exists = openurl_db_has_setting($name);
  if ($exists) {
    variable_set('openurl_default', $name);
    $message = 'Successfully changed the default OpenURL to "%name"';
    $type = 'status';
  }
  else {
    $message = 'Failed to change the default OpenURL to "%name"';
    $type = 'error';
  }
  $message_args = array('%name' => $name);
  drupal_set_message(t($message, $message_args), $type);
}

/**
 * Display the form for adding parameters.
 *
 * @param array $form_state
 *   Drupal form state.
 * @param string $name
 *   The name of the settings to edit.
 *
 * @return array
 *   Drupal form definition.
 */
function openurl_default_param_form(array &$form_state, $name) {
  $form = array();
  if (openurl_db_has_setting($name)) {
    $default_parameters = array(
      '#header' => array('Key', 'Value'),
      '#theme' => THEME_OPENURL_FORM_TABLE,
    );
    $results = openurl_db_get_default_params($name);
    while ($row = db_fetch_object($results)) {
      $default_parameters[] = array(
        '#tree' => TRUE,
        'key' => array('#value' => $row->param_key),
        'value' => array('#value' => $row->param_value),
        'delete' => array(
          '#type' => 'submit',
          '#value' => 'Delete',
          '#parameter_id' => $row->id,
          '#key' => $row->param_key,
          '#name' => 'delete-' . $row->id,
          '#submit' => array('openurl_delete_default_parameter'),
        )
      );
    }
  }
  $add = array(
    '#type' => 'submit',
    '#value' => 'Add',
    '#submit' => array('openurl_add_default_param'),
  );
  $default_parameters['add'] = array(
    'add_key' => array(
      '#type' => 'textfield',
      '#title' => 'Key',
      '#size' => 32,
      '#default_value' => isset($form_state['values']['add_key']) ? $form_state['values']['add_key'] : NULL,
    ),
    'add_value' => array(
      '#type' => 'textfield',
      '#title' => 'Value',
      '#size' => 32,
      '#default_value' => isset($form_state['values']['add_value']) ? $form_state['values']['add_value'] : NULL,
    ),
    'submit' => $add
  );
  $form = array(
    '#attributes' => array('enctype' => 'multipart/form-data'),
    'setting' => array(
      '#type' => 'hidden',
      '#value' => $name,
    ),
    'fieldset' => array(
      '#title' => t('Default Parameters for %name', array('%name' => $name)),
      '#type' => 'fieldset',
      /**
       * A bit of a hack, I want the default action here to be add not delete, so I've mirrored the add button and
       * placed it at the begining of the form. Making it hidden, so it will be the default action.
       */
      'add' => array_merge($add, array('#attributes' => array('style' => 'position: absolute; left: -9999px; width: 1px; height: 1px;'))),
      'settings' => $default_parameters
    ),
  );
  return $form;
}

/**
 * Adds a default key value pair for the given setting.
 *
 * @param array $form_state
 *   Drupal form state.
 * @param string $name
 *   The name of the settings to edit.
 */
function openurl_add_default_param(array $form, array &$form_state) {
  $values = $form_state['values'];
  if (openurl_db_add_default_param($values['setting'], $values['add_key'], $values['add_value'])) {
    $message = 'Successfully added default parameter "%key" for %name';
    $type = 'status';
  }
  else {
    $message = 'Failed to add default parameter "%key" for "%name"';
    $type = 'error';
  }
  $message_args = array('%key' => $values['add_key'], '%name' => $values['setting']);
  drupal_set_message(t($message, $message_args), $type);
}

/**
 * Deletes the given default parameter.s
 *
 * @param array $form_state
 *   Drupal form state.
 * @param string $name
 *   The name of the settings to edit.
 */
function openurl_delete_default_parameter(array $form, array &$form_state) {
  $id = $form_state['clicked_button']['#parameter_id'];
  $key = $form_state['clicked_button']['#key'];
  $setting = $form_state['values']['setting'];
  if (openurl_db_delete_default_param($id)) {
    $message = 'Successfully removed the default parameter "%key" for "%name"';
    $type = 'status';
  }
  else {
    $message = 'Failed to removed the default parameter "%key" for "%name"';
    $type = 'error';
  }
  $message_args = array('%key' => $key, '%name' => $setting);
  drupal_set_message(t($message, $message_args), $type);
}

/**
 * Theme's a form table for this module.
 *
 * @param array $element
 *   A Drupal Form Element.
 *
 * @return sting
 *   HTML that renders a table of settings for datastreams.
 */
function theme_openurl_form_table(array $element) {
  $rows = array();
  foreach (element_children($element) as $child) {
    $setting = $element[$child];
    $fields = array();
    foreach (element_children($setting) as $property) {
      $field = $setting[$property];
      $fields[] = drupal_render($field);
    }
    $rows[] = array(
      'data' => $fields,
      'class' => isset($setting['#attributes']['class']) ? $setting['#attributes']['class'] : NULL
    );
  }
  $attributes = isset($element['#id']) ? array('id' => $element['#id']) : NULL;
  return theme_table($element['#header'], $rows, $attributes);
}

/**
 * Renders both admin forms, used for editing existing OpenURL settings.
 *
 * @param string $name
 *   The name of the OpenURL settings.
 *
 * @return string
 *   Returns the html for both admin forms.
 */
function theme_openurl_form_default_parameters($name) {
  if (!openurl_db_has_setting($name)) {
    drupal_not_found();
    return;
  }
  else {
    $output = drupal_get_form('openurl_settings_form');
    $output .= drupal_get_form('openurl_default_param_form', $name);
    return $output;
  }
}