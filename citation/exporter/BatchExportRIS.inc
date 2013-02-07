<?php

/**
 * @file
 *
 * Functions for generating a batch export of citations to a single RIS file.
 */

/**
 * Gets the operations required to batch export a collection of citations to an RIS file.
 * 
 * @param array $pids 
 *   The citation objects to convert into an RIS file.
 * 
 * @return array 
 *   The operations required to generate a RIS file from the given citations.
 */
function citation_exporter_batch_get_ris_operations(array $pids) {
  $temp_file_dir = file_directory_temp();
  $temp_mods_file = file_create_filename("mods.xml", $temp_file_dir);
  $temp_pids_file = file_create_filename("pids.txt", $temp_file_dir);
  
  citation_exporter_batch_store_pids_in_file($temp_pids_file, $pids);
  $total = count($pids);
  $mods = new DOMDocument();
  $mods->loadXML('<modsCollection xmlns="http://www.loc.gov/mods/v3" />');
  $mods->save($temp_mods_file);
  $operations = array();
  $operations[] = array('citation_exporter_batch_combine_mods', array($temp_pids_file, $total, $temp_mods_file));
  $operations[] = array('citation_exporter_batch_convert_mods_to_ris', array($temp_mods_file));
  $operations[] = array('citation_exporter_batch_redirect', array($_GET['q']));
  return $operations;
}

function citation_exporter_batch_combine_mods($pids_filename, $total, $mods_filename, array &$context) {
  if (empty($context['sandbox'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['total'] = $total;
  }
  list($pids, $remaining) = citation_exporter_batch_get_num_pids_from_file($pids_filename, 5);
  _citation_exporter_batch_combine_mods($mods_filename, $pids);
  $context['sandbox']['progress'] = $context['sandbox']['total'] - $remaining;
  if ($context['sandbox']['progress'] != $context['sandbox']['total']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['total'];
  }
}

function _citation_exporter_batch_combine_mods($filename, array $pids) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_item');
  $mods = new DOMDocument();
  $mods->load($filename);

  foreach ($pids as $pid) {
    //this code could use some clean up.
    $item = new Fedora_Item($pid);
    if ($item->exists() && isset($item->datastreams['MODS'])) {
      $datastream = $item->get_datastream_dissemination('MODS');
      $datastream = trim($datastream);
      
      //add PID to mods:identifier @type="id export" element. zotero and endnote add this field with their internal refernence
      $mods_tmp = new SimpleXMLElement($datastream);
      $mods_tmp->registerXPathNamespace('mods', 'http://www.loc.gov/mods/v3');
      $id = $mods_tmp->addChild('identifier', $pid);
      $id->addAttribute('type', 'id export');
      
      //combine stractured abstracts into one element called displayLabel='combined-all-export'
      $abstract_labels = $mods_tmp->xpath("//mods:mods/mods:abstract[@displayLabel]/@displayLabel");
      $abstract_labels = array_map(function ($str) { return "$str: "; }, $abstract_labels);
      $abstracts = $mods_tmp->xpath("//mods:mods/mods:abstract[@displayLabel]/text()");
      $text = "";
      foreach($abstracts as $key => $abstract) {
        $text .= $abstract_labels[$key] . " $abstract ";
      }
      $abstract_all = $mods_tmp->addChild('abstract', $text);
      $abstract_all->addAttribute('displayLabel', 'combined-all-export');
      
      $datastream = $mods_tmp->asXML();
      
      if (!empty($mods)) {
        $doc = new DOMDocument();
        $doc->loadXML($datastream);
        $node = $mods->importNode($doc->documentElement, TRUE);
        $mods->documentElement->appendChild($node);
      }
    }
  }
  $mods->save($filename);  
}



/**
 * Converts the given MODS file to RIS
 * 
 * @param string $filename
 *   The name of the MODS file to convert to RIS.
 * @param array $context
 *   The batch processes context.
 */
function citation_exporter_batch_convert_mods_to_ris($mods_filename, array &$context) {
  $temp_file_dir = file_directory_temp();
  $ris_filename = file_create_filename("ris_export.ris", $temp_file_dir);
  
  module_load_include('inc', 'citation_exporter', 'class_ris');
  module_load_include('inc', 'citation_exporter', 'ris_map');
  $ris = new ris(file_get_contents($mods_filename), field_map());   
  $text = $ris->render();
  $ris_file = fopen($ris_filename, 'a');
  fwrite($ris_file, $text);
  fclose($ris_file);
  
  $context['results']['filename'] = $ris_filename;
  $context['results']['mime_type'] = 'application/x-Research-Info-Systems';
  $context['results']['download_filename'] = 'export.ris';
}