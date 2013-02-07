<?php

/**
 * @file
 *
 */
module_load_include('inc', 'islandora_solr_search', 'IslandoraSolrResults');

/**
 * Extention of IslandoraSolrResults for templating purposes.
 * This overrides the displayResults function to provide an alternate display type.
 */
class IslandoraSolrResultsCitation extends IslandoraSolrResults {

  /**
   * Overrides parent function.
   * 
   * @param IslandoraSolrQueryProcessor $query_processor
   * @param string $title
   * @param string $output 
   */
  function displayResults(IslandoraSolrQueryProcessor $query_processor, $title = "Search Results", $output = '') {
    module_load_include('inc', 'scholar', 'SearchForm');
    drupal_set_title(t('Search Publications'));
    drupal_add_css(drupal_get_path('module', 'scholar') . '/css/Search.css');
    if (!empty($_SESSION['scholar']['search']['sort'])) {
      $sort = $_SESSION['scholar']['search']['sort'];
      $order = $_SESSION['scholar']['search']['order'];
      $query_processor->solrParams['fl'] = 'PID,mod_fullxml_store';
      $query_processor->solrParams['sort'] = $sort . ' ' . $order;
      $query_processor->executeQuery();
    }
    $total = (int) $query_processor->solrResult->response->numFound;
    $output .= drupal_get_form('scholar_search_form');
    if ($total === 0) {
      $output .= "<h2>Search Results</h2>";
      $output .= "<div>Sorry, but your search returned no results.</div>";
      return $output;
    }
    islandora_solr_search_pager_init($total, $query_processor->solrLimit);
    $output .= '<div class="content">';
    $output .= drupal_get_form('scholar_search_results_form', $query_processor);
    $output .= theme('pager', null, $query_processor->solrLimit, 0, null, 5);
    $output .= "</div>";
    return $output;
  }

  /**
   * Function: printResults
   *
   * Description: translates a solr query result into a basic in-browser search result.
   *
   * @param Apache_Solr_Object $results
   */
  function printResults($results) {
    module_load_include('inc', 'fedora_repository', 'api/fedora_item');
    if (empty($results)) {
      return "no results";
    }
    $output = '';
    $record_start = $results->response->start;
    $limit_results = variable_get('islandora_solr_search_limit_result_fields', 0);
    module_load_include('inc', 'csl', 'CSL');
    $style = citeproc_default_style();
    foreach ($results->response->docs as $doc) {
      $output .= theme(THEME_SCHOLAR_SEARCH_ITEM, $doc->PID, $style);
    }
    return $output;
  }

}

function template_preprocess_scholar_search_alt_item(array &$vars) {
  $s_doc = & $vars['solr_doc'];
  $pid = $s_doc->PID;
  $title = $s_doc->mods_title_ms;
  if (is_array($title)) {
    $title = $title[0];
  }

  $vars['title'] = l($title, 'fedora/repository/' . htmlspecialchars($pid, ENT_QUOTES, 'utf-8'));

  $vars['citation'] = scholar_citation_from_solr_doc($s_doc, $vars['style']);
}

/**
 *
 * @param array $variables 
 */
function template_preprocess_scholar_search_item(array &$variables) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $pid = $variables['pid'];
  $item = new Fedora_Item($pid);
  if ($item->exists() && isset($item->datastreams['MODS'])) {
    $title = $item->objectProfile->objLabel;
    $variables['title'] = l($title, 'fedora/repository/' . htmlspecialchars($pid, ENT_QUOTES, 'utf-8'));

    $style = $variables['style'];
    $cache_id = "citation_{$pid}";
    $cached_raw = cache_get($cache_id);

    $bibliography = NULL;
    if (!empty($cached_raw)) {
      $bibliography = citeproc_bibliography_from_raw_array($style, $cached_raw->data);
    }
    else {
      $mods = $item->get_datastream_dissemination('MODS');
      $mods = trim($mods);
      if (!empty($mods)) {
        $doc = new DOMDocument();
        $doc->loadXML($mods);

        $bibliography = citeproc_bibliography_from_mods($style, $doc, $cache_id);
      }
    }
    $variables['citation'] = $bibliography->render();
  }
}

function scholar_search_results_form(array &$form_state, IslandoraSolrQueryProcessor $query_processor) {
  global $user;
  citation_exporter_include_download_redirect();
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $results = $query_processor->solrResult;
  $total = (int) $results->response->numFound;
  $start = (int) $query_processor->solrStart + 1;
  $end = min(($query_processor->solrLimit + $query_processor->solrStart), $total);
  $title = t('Search Results (%start - %end of %total)', array('%start' => $start, '%end' => $end, '%total' => $total));
  $table = array(
    '#header' => array(theme('table_select_header_cell'), $title),
    '#theme' => 'scholar_search_results_table',
    '#tree' => TRUE,
  );

  module_load_include('inc', 'csl', 'CSL');
  $style = citeproc_default_style();
  $pids = array();
  foreach ($results->response->docs as $doc) {
    $pid = $doc->PID;
    $result = '';
    $pids[] = $pid;
    $result = theme(THEME_SCHOLAR_SEARCH_ITEM, $pid, $style);
    $table['rows'][] = array(
      '#pid' => $pid,
      'result' => array('#value' => $result),
    );
  }
  $checkboxes = array_fill_keys($pids, '');
  $table['selections'] = array(
    '#type' => 'checkboxes',
    '#options' => $checkboxes,
  );
  $form_state['storage']['query'] = $query_processor;
  $options = array(
    'RIS' => 'RIS (Endnote format)',
    'RTF' => 'RTF (word proccessor format)',
    'PDF' => 'PDF',
  ); //'CSV' => 'CSV'
  if ($user->uid == 1) { // Only admin can do.
    $options['CSV'] = 'CSV';
  }
  return array(
    'controls' => array(
      '#type' => 'markup',
      '#prefix' => '<div class="scholar-export-controls">',
      '#suffix' => '</div>',
      'export' => array(
        '#value' => t('Export To File'),
        '#prefix' => '<h4>',
        '#suffix' => '</h4>'
      ),
      'format' => array(
        '#type' => 'select',
        '#length' => 50,
        '#options' => $options
      ),
      'style' => array(
        '#type' => 'select',
        '#options' => CSL::GetNames()
      ),
      'export all' => array(
        '#type' => 'submit',
        '#action' => 'export',
        '#export' => 'all',
        '#value' => t('Export All Results'),
        '#submit' => array('scholar_search_results_form_export')
      ),
      'export selected' => array(
        '#type' => 'submit',
        '#action' => 'export',
        '#export' => 'selected',
        '#value' => t('Export Selected'),
        '#submit' => array('scholar_search_results_form_export')
      ),
      'save' => array(
        '#value' => t('Save to Bibliography'),
        '#prefix' => '<h4>',
        '#suffix' => '</h4>'
      ),
      'save all' => array(
        '#type' => 'submit',
        '#save' => 'all',
        '#value' => t('Save All Results'),
        '#submit' => array('scholar_search_results_form_save_to_bibliography')
      ),
      'save selected' => array(
        '#type' => 'submit',
        '#save' => 'selected',
        '#value' => t('Save Selected'),
        '#submit' => array('scholar_search_results_form_save_to_bibliography')
      ),
    ),
    'pager' => array(
      '#value' => theme('pager', null, $query_processor->solrLimit, 0, null, 5)
    ),
    'table' => $table,
  );
}

/**
 * Check to see if some thing has been selected.
 */
function scholar_search_results_form_validate(array $form, array &$form_state) {
// @todo prevent submission with more then.
}

/**
 *
 * @param array $form_state
 * @return type 
 */
function _scholar_search_get_all_pids_from_query(array &$form_state) {
  $query_processor = $form_state['storage']['query'];
  $previous_limit = $query_processor->solrLimit;
  $query_processor->solrLimit = 100000;
  $query_processor->solrParams['fl'] = 'PID';
  $query_processor->executeQuery();
  unset($query_processor->solrParams['fl']);
  $query_processor->solrLimit = $previous_limit;
  $results = $query_processor->solrResult;
  $pids = array();
  foreach ($results->response->docs as $doc) {
    $pids[] = $doc->PID;
  }
  return $pids;
}

function scholar_search_results_form_save_to_bibliography(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_bibliography', 'Bibliography');
  $action = $form_state['clicked_button']['#save'];
  if ($action == 'all') {
    $pids = _scholar_search_get_all_pids_from_query($form_state);
  }
  else {
    $pids = $form_state['values']['table']['selections'];
    $pids = array_filter($pids);
    $pids = array_keys($pids);
  }
  $count = Bibliography::AddCitations($pids);
  $form_state['storage'] = NULL;
  if ($count > 0) {
    drupal_set_message(t('Added %num Citation(s)', array('%num' => $count)));
  }
  $existed = count($pids) - $count;
  if ($existed > 0) {
    drupal_set_message(t('%num Citation(s) Already Present', array('%num' => $existed)));
  }
}

function scholar_search_results_form_export(array $form, array &$form_state) {
  $action = $form_state['clicked_button']['#export'];
  if ($action == 'all') {
    $pids = _scholar_search_get_all_pids_from_query($form_state);
  }
  else {
    $selections = array_filter($form_state['values']['table']['selections']);
    $pids = array_keys($selections);
  }
  if ($form_state['values']['format'] == 'CSV') {
    scholar_search_results_form_export_csv($form, $form_state);
  }
  else {
    module_load_include('inc', 'citation_exporter', 'Batch');
    citation_exporter_batch_export($pids, $form_state['values']['style'], $form_state['values']['format']);
    batch_process();
  }
  unset($form_state['storage']);
}

function scholar_search_results_form_export_csv(array $form, array &$form_state) {
  if ($form_state['values']['format'] == 'CSV') {
    module_load_include('inc', 'islandora_solr_config', 'IslandoraSolrResultsCSV');
    $query_processor = $form_state['storage']['query'];
    $solrParams = $query_processor->solrParams; // Copy old params
    $fields = array(
      'PID',
      'fgs.label',
      'fgs.createdDate',
      'fgs.lastModifiedDate',
      'coh_search_abstract',
      'coh_search_author',
      'coh_search_author_affiliation',
      'coh_search_author_identifier',
      'coh_search_biological_pathway',
      'coh_search_book_authors',
      'coh_search_book_group_authors',
      'coh_search_book_title',
      'coh_search_cancer_center_membership_level',
      'coh_search_cancer_center_program_code',
      'coh_search_chemical_or_substance_name',
      'coh_search_collection_series_title',
      'coh_search_conference',
      'coh_search_cores',
      'coh_search_dept_id',
      'coh_search_dept_name',
      'coh_search_disease_program',
      'coh_search_division_id',
      'coh_search_division_name',
      'coh_search_doi',
      'coh_search_editor',
      'coh_search_full_journal_title',
      'coh_search_funding_agency',
      'coh_search_grant_number',
      'coh_search_group_author',
      'coh_search_isbn',
      'coh_search_issn',
      'coh_search_keywords',
      'coh_search_language',
      'coh_search_mesh_terms',
      'coh_search_osr_id_number',
      'coh_search_peer_reviewed',
      'coh_search_publication_type',
      'coh_search_secondary_source_id',
      'coh_search_status',
      'coh_search_times_cited',
      'coh_search_title',
      'coh_search_tumor_model',
      'coh_search_year_published',
      'coh_facet_keywords',
      'coh_facet_biological_pathway',
      'coh_facet_cancer_center_membership_level',
      'coh_facet_cancer_center_program_code',
      'coh_facet_cores',
      'coh_facet_dept_name',
      'coh_facet_disease_program',
      'coh_facet_division_name',
      'coh_facet_full_journal_title',
      'coh_facet_funding_agency',
      'coh_facet_grant_number',
      'coh_facet_language',
      'coh_facet_mesh_terms',
      'coh_facet_peer_reviewed',
      'coh_facet_publication_date',
      'coh_facet_publication_type',
      'coh_facet_status',
      'coh_facet_tumor_model',
      'coh_facet_year_published',
    );
    $query_processor->solrParams['fl'] = implode(',', $fields);
    $results = new IslandoraSolrResultsCSV();
    $results->printCSV($query_processor);
    $query_processor->solrParams = $solrParams;
  }
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
function theme_scholar_search_results_table(array $element) {
  $rows = array();
  foreach (element_children($element['rows']) as $child) {
    $setting = $element['rows'][$child];
    $pid = $setting['#pid'];
    $fields = array(
      drupal_render($element['selections'][$pid]) // First field is a checkbox
    );
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
  return '<div class="scholar-search-results">' . theme_table($element['#header'], $rows, $attributes) . '</div>';
}

