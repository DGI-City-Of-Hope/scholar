<?php

/**
 * @file
 *
 * Functions for including the Manage Bibliography form.
 */

/**
 * This form is called from the menu callback for the 'My Bibliography' page.
 *
 * From here the user can add/remove and export citations.
 *
 * @param array $form_state
 *   The drupal form state.
 *
 * @return array
 *   The drupal form.
 */
function islandora_bibliography_form(array &$form_state) {
  global $pager_page_array, $pager_total, $pager_total_items;
  module_load_include('inc', 'islandora_bibliography', 'Bibliography');
  $page = isset($_GET['page']) ? trim($_GET['page']) : '';
  citation_exporter_include_download_redirect();
  $pids = Bibliography::GetCitations();
  $limit = 100;
  $total = count($pids);
  $pager_page_array = array($page);
  $pager_total_items[0] = $total;
  $pager_total[0] = ceil($pager_total_items[0] / $limit);
  $pager_page_array[0] = $page = max(0, min((int)$pager_page_array[0], ((int)$pager_total[0]) - 1));
  $start = $page * $limit;
  if (empty($pids)) {
    islandora_bibilgraphy_form_empty_page();
  }
  else {
    module_load_include('inc', 'csl', 'CSL');
    drupal_add_css(drupal_get_path('module', 'islandora_bibliography') . '/css/bibliography.css');
    $styles = CSL::GetNames();
    return array(
      'fieldset' => array(
        '#type' => 'fieldset',
        '#title' => t('My Citations'),
        '#description' => t('<p>Here you can remove or export citations from your bibliography.</p>'),
        '#tree' => TRUE,
        'preview_export_top' => _islandora_bibliography_export_preview_form_stuff($styles),
        'table' => islandora_bibilgraphy_form_table($pids, $total, $start, $limit),
        'pager' => array(
          '#value' => theme('pager', null, $limit, 0, null, 5)
        ),
        'preview_export_bottom' => _islandora_bibliography_export_preview_form_stuff($styles),
      )
    );
  }
}

function _islandora_bibliography_export_preview_form_stuff($styles) {
static $i = 0;
$i++;
return array(
          '#type' => 'fieldset',
          //'#title' => t('Preview/Export'),
          '#attributes' => array(
            'style' => 'border: none;',
          ),
          'preview_fs' => array(
            '#type' => 'fieldset',
            '#title' => t('Preview/Remove Citations'),
            'style' => array(
              '#title' => t('Style'),
              '#type' => 'select',
              '#options' => $styles
            ),
            'preview' => array(
              '#type' => 'submit',
              '#value' => t('Preview Selected'),
              '#submit' => array('islandora_bibliography_form_preview'),
	      '#name' => "preview_$i",            ),
            'remove' => array(
              '#type' => 'submit',
              '#value' => t('Remove Selected'), 
              '#submit' => array('islandora_bibliography_form_remove')
            ),
          ),
          'export_fs' => array(
            '#type' => 'fieldset',
            '#title' => t('Export To File'),
            'format' => array(
              '#title' => t('Format'),
              '#type' => 'select',
              '#length' => 50,
              '#options' => array(
                'RIS' => 'RIS (Endnote format)',
                'RTF' => 'RTF (word proccessor format)',
                'PDF' => 'PDF',
              )
            ),
            'style' => array(
              '#title' => t('Style'),
              '#type' => 'select',
              '#options' => $styles
            ),
            'export' => array(
              '#type' => 'submit',
              '#action' => 'export',
              '#export' => 'selected',
              '#value' => t('Export Selected'),
              '#submit' => array('islandora_bibliography_form_export'),
	      '#name' => "export_$i"
            )
          )
        );
}

/**
 * The form to render when no citations are in the bibliography.
 *
 * @return array
 *   The drupal form
 */
function islandora_bibilgraphy_form_empty_page() {
  drupal_set_message(t('You have no citations in your bibliography.'));
  return array(
    'redirect' => array(
      '#prefix' => '<p>',
      '#value' => l(t('Return to the Home Page.'), ''),
      '#suffix' => '</p>'
    )
  );
}

/**
 * Generates the table for the bibliography form.
 *
 * @param array $pids
 *   A collection of citations within the bibliography.
 * @param int $total
 *   The total number of pids.
 * @param int $start
 *   The index of the first pid to render.
 * @param int $limit
 *   The number of results in the table.
 *
 * @return array
 *   The drupal form table definition.
 */
function islandora_bibilgraphy_form_table(array $pids, $total, $start = 0, $limit = 10) {
  $table = array(
    '#header' => array(theme('table_select_header_cell'), array('data' => t('Title')), array('data' => t('Author(s)')), array('data' => '', 'colspan' => 1)),
    '#theme' => THEME_BIBLIOGRAPHY_FORM_TABLE,
    '#tree' => TRUE,
    'rows' => array(),
    'selections' => array(
      '#type' => 'checkboxes',
      '#options' => array_fill_keys($pids, ''),
    ),
  );
  $row_count = 0;
  $rows = &$table['rows'];
  $pids = array_splice($pids, $start, $limit);
  foreach ($pids as $pid) {
    list($title, $authors) = islandora_bibilgraphy_form_get_title_authors($pid);
    $rows[] = array(
      '#pid' => $pid,
      'title' => array('#value' => l(check_plain($title), "fedora/repository/$pid")),
      'author' => array('#value' => check_plain($authors)),
      'remove' => array(
        '#type' => 'submit',
        '#value' => t('Remove'),
        '#name' => 'remove-citation' . $row_count, // Give a unique name to each button
        '#pid' => $pid,
        '#submit' => array('islandora_bibliography_form_remove_citation_submit')
      )
    );
    $row_count++;
  }
  return $table;
}

/**
 * Get the title and author of a given citation.
 *
 * @param string $pid
 *   A citations within the bibliography.
 *
 * @return array
 *   Where the first key is the title and the second is the authors.
 */
function islandora_bibilgraphy_form_get_title_authors($pid) {
  $title = $authors = 'Missing';
  $mods = islandora_bibilgraphy_form_get_mods($pid);
  if ($mods) {
    $mods = simplexml_import_dom($mods);
    $mods->registerXPathNamespace('mods', 'http://www.loc.gov/mods/v3');
    $title = implode(' ', $mods->xpath('/mods:mods/mods:titleInfo/mods:title'));
    $authors = implode(', ', $mods->xpath('/mods:mods/mods:name/mods:role[mods:roleTerm = "author"]/../mods:namePart[@type="family"]'));
  }
  return array($title, $authors);
}

/**
 * Removes one citation from the bibliography.
 *
 * @param array $form
 *   The drupal form.
 * @param array $form_state
 *   The drupal form state.
 */
function islandora_bibliography_form_remove_citation_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_bibliography', 'Bibliography');
  $pid = $form_state['clicked_button']['#pid'];
  Bibliography::RemoveCitation($pid);
  drupal_set_message(t('Removed Citation'));
}

function _islandora_bibliography_get_selections($form_state) {
  $selections = $form_state['values']['fieldset']['table']['selections'];
  return array_keys(array_filter($selections));
}

/**
 *  Get the style by looking relative to the parent of the button.
 */
function _islandora_bibliography_get_style($form_state) {
  $parents = $form_state['clicked_button']['#array_parents'];
  $parents = array_slice($parents, 0, -1);
  $temp = $form_state['values'][array_shift($parents)];
  foreach ($parents as $parent) {
    $temp = $temp[$parent];
  }
  return $temp['style'];
}

/**
 *  Get the format by looking relative to the parent of the button.
 */
function _islandora_bibliography_get_format($form_state) {
  $parents = $form_state['clicked_button']['#array_parents'];
  $parents = array_slice($parents, 0, -1);
  $temp = $form_state['values'][array_shift($parents)];
 foreach ($parents as $parent) {
     $temp = $temp[$parent];
  }
  return $temp['format'];
}

/**
 * Previews the selected citations from the bibliography.
 *
 * @param array $form
 *   The drupal form.
 * @param array $form_state
 *   The drupal form state.
 */
function islandora_bibliography_form_preview(array $form, array &$form_state) {
  module_load_include('inc', 'citeproc', 'CiteProcJSBibliography');
  drupal_add_css(drupal_get_path('module', 'islandora_bibliography') . '/css/export.css');
  $selections = _islandora_bibliography_get_selections($form_state);
  $style_name = _islandora_bibliography_get_style($form_state);
  $style = citeproc_style($style_name);
  $bibliography = new CiteProcJSBibliography($style);
  foreach ($selections as $pid) {
    $mods = islandora_bibilgraphy_form_get_mods($pid);
    if ($mods) {
      $citation = citeproc_citation_from_mods($mods);
      $bibliography->addCitation($citation);
    }
  }
  $output = $bibliography->render();
  $output .= l(t('Return to the Bibliography.'), 'bibliography');
  
  print theme('page', $output, FALSE);
  exit();
}

/**
 * Removes the selected citations from the bibliography.
 *
 * @param array $form
 * @param array $form_state
 */
function islandora_bibliography_form_remove(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_bibliography', 'Bibliography');
  $pids = _islandora_bibliography_get_selections($form_state);
  Bibliography::RemoveCitations($pids);
  drupal_set_message(t('Removed %num Citation(s)', array('%num' => count($pids))));
}

/**
 * Exports the selected pids to file.
 *
 * @param array $form
 * @param array $form_state
 */
function islandora_bibliography_form_export(array $form, array &$form_state) {
  module_load_include('inc', 'citation_exporter', 'Batch');
  $pids = _islandora_bibliography_get_selections($form_state);
  citation_exporter_batch_export($pids, _islandora_bibliography_get_style($form_state), _islandora_bibliography_get_format($form_state));
  batch_process();
}

/**
 * Gets the MOD from an object.
 *
 * Helper function for getting the MODS metadata.
 *
 * @param string $pid
 *   The fedora object to get the MODS from.
 *
 * @return DOMDocument
 *   An MODS document if found and non-empty, FALSE otherwise.
 */
function islandora_bibilgraphy_form_get_mods($pid) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $item = new Fedora_Item($pid);
  if ($item->exists() && isset($item->datastreams['MODS'])) {
    $mods = $item->get_datastream_dissemination('MODS');
    $mods = trim($mods);
    if (!empty($mods)) {
      $doc = new DOMDocument();
      $doc->loadXML($mods);
      return $doc;
    }
  }
  return FALSE;
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
function theme_islandora_bibliography_form_table(array $element) {
  $rows = array();
  foreach (element_children($element['rows']) as $child) {
    $setting = $element['rows'][$child];
    $pid = $setting['#pid'];
    $fields = array(
      drupal_render($element['selections'][$pid]) // First field is a checkbox
    );
    foreach (element_children($setting) as $property) {
      $field = $setting[$property];
      $fields[] = array('data' => drupal_render($field));
    }
    $rows[] = array(
      'data' => $fields,
      'class' => isset($setting['#attributes']['class']) ? $setting['#attributes']['class'] : NULL
    );
  }
  $attributes = isset($element['#id']) ? array('id' => $element['#id']) : NULL;
  return theme_table($element['#header'], $rows, $attributes);
}
