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
      
    $item = new Fedora_Item($pid);
    if ($item->exists() && isset($item->datastreams['MODS'])) {
      $datastream = $item->get_datastream_dissemination('MODS');
      $datastream = trim($datastream);
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
 * Converts the given MODS file to RIS using bibutils
 * 
 * @param string $filename
 *   The name of the MODS file to convert to RIS.
 * @param array $context
 *   The batch processes context.
 */
function citation_exporter_batch_convert_mods_to_ris($mods_filename, array &$context) {
    //dsm($mods_filename,"MODS FILKE NAME");
    //dsm(file_get_contents($mods_filename),"MODS FILE content");
  module_load_include('inc', 'bibutils', 'Bibutils');
  $temp_file_dir = file_directory_temp();
  $ris_filename = file_create_filename("ris_export.ris", $temp_file_dir);
  
    //HACK
    //Bibutils doesn't consistently export every record... so break the mods collection apart and run one them 
    //one at a run and then combine all the ris records afterwards
    $dom = new DOMDocument;
    $dom->load($mods_filename);
    
    $xpath = new DOMXPath($dom);
    $xpath->registerNamespace('mods', 'http://www.loc.gov/mods/v3');
    $node_list = $xpath->query('//mods:mods');
    $ris_tmp_files = array();
    $i=0;
    
    foreach ($node_list as $dom_element){
        
        $ris_filename_hack = str_replace(".ris", "", $ris_filename)."_$i.ris";
        $mods_filename_hack = str_replace(".xml", "", $mods_filename)."_$i.xml";
        $ris_tmp_files[] = $ris_filename_hack;
        
        $new_dom = new DOMDocument();
        $xpath = new DOMXPath($new_dom);
        $xpath->registerNamespace('mods', 'http://www.loc.gov/mods/v3');
        $root = $new_dom->createElementNS('http://www.loc.gov/mods/v3', 'modsCollection');
        $root = $new_dom->appendChild($root);

        $dom_node = $new_dom->importNode($dom_element, true);
        $root->appendChild($dom_node);
        
        //HACK TO MAKE SURE PlaceTerm has attribute type="text"
        $place_terms = $xpath->query('//mods:mods/mods:originInfo/mods:place/mods:placeTerm[not(@type = "text")]');
        foreach ($place_terms as $place_term) {        
            $dom_attribute = $new_dom->createAttribute('type');
            $dom_attribute->value = 'text';
            $place_term->appendChild($dom_attribute);            
        }
        //HACK MOVE KEYWORDS SO BIBUTILS CAN READ THEM 
        //from mods/note[@type="keywords"] to mods/subject/topic 
        $notes = $xpath->query('//mods:mods/mods:note[@type = "keywords"]');
        foreach ($notes as $note) {
            $topic = $new_dom->createElementNS('http://www.loc.gov/mods/v3', 'topic', $note->nodeValue);
            $subject = $new_dom->createElementNS('http://www.loc.gov/mods/v3', 'subject');
            $subject->appendChild($topic);
            
            $mods = $xpath->query('//mods:mods');
            $mods->item(0)->appendChild($subject);
        }
        
        
        $notes = $xpath->query('//mods:mods/mods:note[@type = "keywords"]');
        
        foreach ($notes as $note) {
            $topic = $new_dom->createElementNS('http://www.loc.gov/mods/v3', 'topic', $note->nodeValue);
            $subject = $new_dom->createElementNS('http://www.loc.gov/mods/v3', 'subject');
            $subject->appendChild($topic);
            
            $mods = $xpath->query('//mods:mods');
            $mods->item(0)->appendChild($subject);
        }
        
        
        $genre = $xpath->query('//mods:mods/mods:genre');
        $host = $xpath->query('//mods:mods/mods:relatedItem[@type = "host"]');
        
        if($host && $genre) {
            $host->item(0)->appendChild($genre->item(0));
        }
        
        $new_dom->save($mods_filename_hack);
        $i++;
        if(Bibutils::Convert($mods_filename_hack, 'MODS', $ris_filename_hack, 'RIS')) {
            //error checking
        }
    }
    
    //combine all export ris files into one for downloading HACK
    $ris_combined_file = fopen($ris_filename, 'a+');
    
    foreach ($ris_tmp_files as $ris_tmp_file) {
        $ris_content = file_get_contents($ris_tmp_file);
        if($ris_content) {
            fwrite($ris_combined_file, $ris_content);
        }
    }
    fclose($ris_combined_file);
   
    $context['results']['filename'] = $ris_filename;
    $context['results']['mime_type'] = 'application/x-Research-Info-Systems';
    $context['results']['download_filename'] = 'export.ris';
}