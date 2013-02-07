<?php

/**
 * @file
 *
 * Contains the hooks for rendering and processing the Browse By User form.
 */

/**
 * Builds a drupal form for launching a browse by user search.
 * 
 * @param array $form_state 
 * 
 * @return array
 */
function scholar_browse_by_user_form(array &$form_state) {
  module_load_include('php', 'islandora_solr_search', 'SolrPhpClient/Apache/Solr/Service');
  $path = parse_url(variable_get('islandora_solr_search_block_url', 'http://localhost:8080/solr/'));
  $solr = new Apache_Solr_Service($path['host'], $path['port'], $path['path']);
  $solr->setCreateDocuments(FALSE);
  $results = $solr->search('*', 0, 1000, array(
    'q.op' => 'AND',
    'df' => 'mods.name_author',
    'fl' => 'PID, mods.name_author',
    'fq' => "rels.hasModel:\"info:fedora/ir:citationCModel\"")
  );
  $users = array();
  foreach ($results->response->docs as &$doc) {
    $authors = $doc->{'mods.name_author'};
    $authors = is_string($authors) ? array($authors) : $authors;
    foreach ($authors as $name) {
      $users[$name] = $name;
    }
  }
  sort($users);
  return array(
    'user' => array(
      '#type' => 'select',
      '#options' => $users
    ),
    'submit' => array(
      '#type' => 'submit',
      '#value' => 'Browse'
    )
  );
}

/**
 * Submit hook for the browse by user form.
 * 
 * @param array $form
 * @param array $form_state 
 */
function scholar_browse_by_user_form_submit(array $form, array &$form_state) {
  $user = $form_state['values']['user'];
  $user = $form['user']['#options'][$user]; // Get proper value.
  $query = "mods.name_author:\"$user\"";
  $form_state['redirect'] = 'islandora/solr/search/' . urlencode($query) . '/'; // Redirect to the search.
}